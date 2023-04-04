#include "torrent.hpp"
#include <iostream>
#include <bencode.hpp>
#include <message.hpp>
#include <unordered_set>

using namespace std;

SingleFileTorrent::SingleFileTorrent(const string_view &data) {
    const auto dict = get<bencode::dict_view>(bencode::decode_view(data));
    // torrents are *supposed* to have announce strings. in practice, they might not.
    announce_ = get<bencode::string_view>(dict.at("announce"));

    // extract info
    const auto info = get<bencode::dict_view>(dict.at("info"));
    piece_length_ = get<bencode::integer_view>(info.at("piece length"));
    file_length_ = get<bencode::integer_view>(info.at("length"));
    filename_ = get<bencode::string_view>(info.at("name"));

    // put piece hashes into a vector
    piece_hashes_.reserve(file_length_ / piece_length_ + 1);
    const auto piece_hashes = get<bencode::string_view>(info.at("pieces"));
    for (auto it = piece_hashes.cbegin(); it < piece_hashes.cend(); it += cmn::HASH_SIZE) {
        piece_hashes_.emplace_back(it);
    }

    // re-encode info dict, so we can compute its hash. as far as I can tell this bencode lib
    // doesn't have a more efficient way to do it
    const auto info_str = bencode::encode(info);
    info_hash_ = cmn::Hash::of(info_str);
}

TrackerResponse::TrackerResponse(const string_view& data) {
    const auto dict = get<bencode::dict_view>(bencode::decode_view(data));
    interval_ = get<bencode::integer_view>(dict.at("interval"));

    const auto peers = get<bencode::string_view>(dict.at("peers"));
    peers_.reserve(peers.length() / 6);
    for (size_t i = 0; i < peers.length(); i += 6) {
        peers_.emplace_back(cmn::Address{peers.substr(i, i + 6).data()});
    }
}

Peer::Peer(const TorrentContext& ctx, cmn::Address addr)
        : addr_(addr), socket_(ctx.io), handshake_(ctx.handshake), tor_(ctx.tor),
        work_queue_(ctx.work_queue), result_queue_(ctx.result_queue)
{
    tcp::resolver resolver(ctx.io);
    auto endpoints = resolver.resolve(addr_.ip(), addr_.port_str());
    ba::async_connect(socket_, endpoints,
                      [this](auto ec, auto _) { shared_from_this()->async_handshake_write(ec); });
}

void Peer::async_handshake_write(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error connecting: " << ec.message() << endl;
        return;
    }

    ba::async_write(socket_, ba::buffer(handshake_),
                    [this](auto ec, auto _) { shared_from_this()->async_handshake_read(ec); });
}

void Peer::async_handshake_read(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error writing handshake: " << ec.message() << endl;
        return;
    }

    // handshake we receive back should be the same length as ours
    recv_buffer_.resize(handshake_.size());
    ba::async_read(socket_, ba::buffer(recv_buffer_), [this](auto ec, auto _) { shared_from_this()->async_next(ec); });
}

void Peer::async_next(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error reading handshake: " << ec.message() << endl;
        return;
    }

    Handshake result{string{recv_buffer_.data(), recv_buffer_.size()}};
    peer_id_ = move(result.peer_id);
    log() << "successfully connected (" << addr_.to_string() << ")" << endl;

    write_message(Message::interested());
    async_read_message();
}

void Peer::async_read_message() {
    // first handle_message the message length
    recv_buffer_.resize(sizeof(uint32_t));
    ba::async_read(socket_, ba::buffer(recv_buffer_),
                    [this](auto ec, auto _) { shared_from_this()->async_read_len(ec); });
}

void Peer::async_read_len(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error reading message length: " << ec.message() << endl;
        if (piece_) while (!work_queue_->push(piece_->index()));
        return;
    }

    uint32_t len = ntohl(*reinterpret_cast<const uint32_t*>(recv_buffer_.data()));
    if (len == 0) {
        // zero length means keepalive message
//        log() << "keepalive" << endl;
        async_read_message();
    } else {
        recv_buffer_.resize(len);
        ba::async_read(socket_, ba::buffer(recv_buffer_),
                       [this](auto ec, auto _) {
           if (ec.failed()) {
               log() << "error reading message from " << ": " << ec.message() << endl;
               if (piece_) while (!work_queue_->push(piece_->index()));
           } else {
               shared_from_this()->handle_message();
           }
        });
    }
}

void Peer::handle_message() {
    Message msg{recv_buffer_};
    cmn::BlockStatus result;

    if (!msg.type.has_value()) {
        return;
    }
    switch (msg.type.value()) {
        case Message::Choke:
            choked_ = true;
            break;
        case Message::Unchoke:
            choked_ = false;
            break;
        case Message::Bitfield:
            if (available_pieces_.size() == 0) {
                available_pieces_.copy_from(msg.payload);
            } else {
                log() << "warning: unexpected Bitfield message" << endl;
            }
            break;
        case Message::Piece:
            if (!piece_) break;
            result = piece_->accept(msg.payload);
            if (result != cmn::BlockStatus::Ok) {
                // this happens a lot due to pipelined piece requests (receive block for piece we already finished)
                if (result != cmn::BlockStatus::WrongPiece && result != cmn::BlockStatus::AlreadyFilled) {
                    log() << "error accepting block: " << cmn::block_status_string(result) << endl;
                }
                release_block();
            } else {
                release_block();
                if (piece_->is_complete()) {
                    auto final_piece = piece_->finalize();
                    auto hash = final_piece.hash();
                    if (hash == tor_.piece_hash(piece_->index())) {
                        while (!result_queue_->push(final_piece));
                    } else {
                        // TODO: track how unreliable this peer is
                        log() << "piece " << piece_->index() << ": failed hash check" << endl;
                        while (!work_queue_->push(piece_->index()));
                    }
                    piece_.reset();
                }
            }
            break;
        default:
            break;
    }
    begin_continue_piece();
    async_read_message();
}

void Peer::begin_continue_piece() {
    if (choked_ || available_pieces_.size() == 0) return;
    if (!piece_) {
        // find a piece to download
        uint32_t index;
        if (!work_queue_->pop(index)) return;
        // if we got a piece that's not available, put it back in the queue and give up
        if (!available_pieces_.get(index)) {
            while (!work_queue_->push(index));
            return;
        } else {
            piece_ = cmn::Piece{index, tor_.piece_size()};
        }
    } else if (piece_->is_complete()) {
        return;
    }

    block_ = piece_->next_block();
    if (!block_) {
        log() << "tried to request block for piece " << piece_->index() << ", but none available" << endl;
        return;
    }
    auto msg = Message::request(piece_.value(), *block_);
    write_message(msg, [this](auto ec, auto _) {
        if (ec.failed()) {
            log() << "failed receiving piece " << piece_->index() << ", block " << block_->index() << ": " << ec.message() << endl;
            release_block();
        }
    });
    active_blocks_ += 1;
    if (active_blocks_ < PIPELINE_LIMIT) begin_continue_piece();
}

void write_thread(const TorrentContext &ctx) {
    auto stream = ofstream{ctx.tor.filename()};
    for (size_t i = 0; i < ctx.tor.file_length(); ++i) {
        stream.put(0);
    }

    double bytes_downloaded = 0;
    unordered_set<uint32_t> missing_pieces;
    for (uint32_t i = 0; i < ctx.tor.pieces(); ++i) {
        missing_pieces.insert(i);
    }

    auto start = std::chrono::system_clock::now();
    auto now = start;
    double elapsed = 0;
    auto last_report = start;

    while (!missing_pieces.empty()) {
        cmn::CompletePiece piece;
        while (!ctx.result_queue->pop(piece));

        stream.seekp(piece.offset());
        stream.write(piece.data(), piece.size());

        piece.free();
        missing_pieces.erase(piece.index());
        bytes_downloaded += piece.size();

        now = std::chrono::system_clock::now();
        auto elapsed_report =
                static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last_report).count());
        if (elapsed_report < 500) continue;

        last_report = now;
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count()) / 1000;

        auto downloaded = bytes_downloaded / 1024;
        auto percent = downloaded / (ctx.tor.file_length() / 1024);
        auto est_duration = elapsed / percent;
        std::cout << "[writer] "
                  << (ctx.tor.pieces() - missing_pieces.size()) << "/" << ctx.tor.pieces()
                  << " pieces complete in "
                  << elapsed << " sec., "
                  << downloaded / elapsed << "kB/s, "
                  << (missing_pieces.size() == 0 ? 0 : est_duration - elapsed) << " sec. remaining"
                  << std::endl;

        // TODO: cool visualisation?
        if (missing_pieces.size() > 0 && missing_pieces.size() < 10) {
            std::cout << "[writer] missing pieces: ";
            for (auto i: missing_pieces) {
                std::cout << i << " ";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "[writer] download complete in "
              << elapsed << " sec. (" << (ctx.tor.file_length() / (1024 * 1024)) / elapsed << "MB/s)!"
              << std::endl;
}
