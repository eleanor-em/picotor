//
// Created by Eleanor McMurtry on 05.04.23.
//
#include <unordered_set>

#include <boost/asio.hpp>

#include <common.hpp>
#include <peer.hpp>
#include <result.hpp>
#include <torrent.hpp>

Peer::Peer(const TorrentContext& ctx, cmn::Address addr)
        : addr_(addr), socket_(ctx.io), ctx_(ctx)
{
    tcp::resolver resolver{ctx.io};
    auto endpoints = resolver.resolve(addr_.ip(), addr_.port_str());
    ba::async_connect(socket_, endpoints,
                      [this](auto ec, auto _) { async_handshake_write(ec); });
}

void Peer::async_handshake_write(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error connecting: " << ec.message() << endl;
        close();
        return;
    }

    ba::async_write(socket_, ba::buffer(ctx_.handshake),
                    [this](auto ec, auto _) { async_handshake_read(ec); });
}

void Peer::async_handshake_read(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error writing handshake: " << ec.message() << endl;
        close();
        return;
    }

    // handshake we receive back should be the same length as ours
    recv_buffer_.resize(ctx_.handshake.size());
    ba::async_read(socket_, ba::buffer(recv_buffer_),
                   [this](auto ec, auto _) { async_next(ec); });
}

void Peer::async_next(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error reading handshake: " << ec.message() << endl;
        close();
        return;
    }

    Handshake result{recv_buffer_};
    peer_id_ = std::move(result.peer_id);
    log() << "successfully connected (" << addr_.to_string() << ")" << endl;
    while (!ctx_.result_queue->push(ResultPeerConnected{addr_}));

    async_write_message(Message::interested());
    async_read_message();
}

void Peer::async_read_message() {
    // first handle_message the message length
    recv_buffer_.resize(sizeof(uint32_t));
    ba::async_read(socket_, ba::buffer(recv_buffer_),
                   [this](auto ec, auto _) { async_read_len(ec); });
}

void Peer::async_read_len(const boost::system::error_code &ec) {
    if (ec.failed()) {
        if (!closed_) log() << "error reading message length: " << ec.message() << endl;
        close();
        return;
    }

    uint32_t len = ntohl(*reinterpret_cast<const uint32_t*>(recv_buffer_.data()));
    if (len == 0) {
        // zero length means keepalive message
        async_read_message();
    } else {
        recv_buffer_.resize(len);
        ba::async_read(socket_, ba::buffer(recv_buffer_),
           [this](auto ec, auto _) {
               if (ec.failed()) {
                   if (!closed_) log() << "error reading message: " << ec.message() << endl;
                   release_piece();
               } else {
                   async_handle_message();
               }
           });
    }
}

void Peer::handle_piece(const Message& msg) {
    if (!piece_) return;

    auto [result, index] = piece_->accept(msg.payload);
    release_block(index);

    if (result != BlockStatus::Ok) {
        // this happens a lot due to pipelined piece requests (receive block for piece we already finished)
        if (result != BlockStatus::WrongPiece && result != BlockStatus::AlreadyFilled) {
            log() << "error accepting block: " << block_status_string(result) << endl;
        }
    } else if (piece_->is_complete()) {
        auto final_piece = piece_->finalize();
        auto hash = final_piece.hash();
        if (hash == ctx_.tor.piece_hash(piece_->index())) {
            while (!ctx_.result_queue->push(ResultPieceComplete{final_piece}));
        } else {
            if (!closed_) log() << "piece " << piece_->index() << ": failed hash check, dropping peer" << endl;
            close();
            return;
        }
        piece_.reset();
    }
}

void Peer::async_handle_message() {
    Message msg{recv_buffer_};
    if (!msg.type) {
        return;
    }

    switch (*msg.type) {
        case Message::Choke:
            choked_ = true;
            break;
        case Message::Unchoke:
            choked_ = false;
            break;
        case Message::Bitfield:
            available_pieces_.copy_from(msg.payload);
            break;
        case Message::Piece:
            handle_piece(msg);
            break;
        default:
            break;
    }
    async_download();
    async_read_message();
}

void Peer::async_download() {
    // make sure we can actually download something first
    if (choked_ || available_pieces_.size() == 0) return;

    if (!piece_) {
        // find a piece to download; yield if we don't succeed right away, to prevent infinite loops
        uint32_t index;
        if (!ctx_.work_queue->pop(index)) return;

        piece_ = Piece{index, ctx_.tor.piece_size()};
        // if we got a piece that's not available from this peer, put it back in the queue and give up
        if (!available_pieces_.get(index)) {
            release_piece();
            return;
        }
    }

    // no point starting a block for a complete piece
    if (piece_->is_complete()) return;

    // find a block to download
    auto block = piece_->next_block();
    if (!block) return;
    blocks_.emplace(block->index(), block);

    // at this point, we have committed to a block; start a timer
    auto expiry = boost::asio::chrono::milliseconds(TIMEOUT_MS);
    auto timer = std::make_shared<boost::asio::steady_timer>(ctx_.io, expiry);
    timer->async_wait([timer, block, this](auto ec) {
        if (ec.failed()) {
            log() << "error in block timeout: " << ec.message() << endl;
            return;
        }
        // give up on this peer
        if (!block->filled()) {
            if (!closed_) log() << "timed out, dropping peer" << endl;
            closed_ = true;
            close();
        }
    });

    // request the block
    auto msg = Message::request(*piece_, *block);
    async_write_message(msg, [block, this](auto ec, auto _) {
        if (ec.failed()) {
            log() << "failed receiving piece " << piece_->index() << ", block " << block->index() << ": " << ec.message() << endl;
            release_block(block->index());
        }
    });

    // if we can request more blocks, go ahead
    if (blocks_.size() < PIPELINE_LIMIT) async_download();
}

typedef std::chrono::time_point<std::chrono::system_clock> Timepoint;

class MonitorVisitor {
public:
    MonitorVisitor(const TorrentContext& ctx)
        : ctx_(ctx), stream_(ofstream{ctx.tor.filename()}) {
        for (size_t i = 0; i < ctx_.tor.file_length(); ++i) {
            stream_.put(0);
        }

        for (uint32_t i = 0; i < ctx_.tor.pieces(); ++i) {
            missing_pieces_.insert(i);
        }
    }

    bool complete() const { return missing_pieces_.empty(); }

    void operator()(ResultPieceComplete result) {
        // write the data
        stream_.seekp(result.piece.offset());
        stream_.write(result.piece.data(), result.piece.size());

        // update piece tracking
        result.piece.free();
        missing_pieces_.erase(result.piece.index());
        bytes_downloaded_ += result.piece.size();

        // update time tracking
        now_ = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now_ - last_report_).count();
        auto elapsed_report = static_cast<double>(duration);

        if (!missing_pieces_.empty() && elapsed_report > 500) {
            report_progress();
        }
    }

    void report_progress() {
        last_report_ = now_;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now_ - start_).count();
        elapsed_ = static_cast<double>(duration) / 1000;

        auto downloaded = bytes_downloaded_ / 1024;
        auto percent = downloaded / (ctx_.tor.file_length() / 1024);
        auto est_duration = elapsed_ / percent;
        log() << (ctx_.tor.pieces() - missing_pieces_.size()) << "/" << ctx_.tor.pieces()
              << " pieces complete in "
              << elapsed_ << " sec., "
              << downloaded / elapsed_ << "kB/s, "
              << (missing_pieces_.size() == 0 ? 0 : est_duration - elapsed_) << " sec. remaining, from "
              << peers_.size() << "/" << ctx_.total_peers << " peers"
              << std::endl;

        // TODO: cool visualisation?
        if (missing_pieces_.size() > 0 && missing_pieces_.size() < 10) {
            log() << "missing pieces: ";
            for (auto i: missing_pieces_) {
                std::cout << i << " ";
            }
            std::cout << std::endl;
        }
    }

    void operator()(ResultPeerConnected result) {
        peers_.emplace(result.addr);
    }

    void operator()(ResultPeerDropped result) {
        peers_.erase(result.addr);
    }

    ~MonitorVisitor() {
        log() << "download complete in "
              << elapsed_ << " sec. (" << (ctx_.tor.file_length() / (1024 * 1024)) / elapsed_ << "MB/s)!"
              << std::endl;

    }
private:
    const TorrentContext& ctx_;
    ofstream stream_;
    unordered_set<uint32_t> missing_pieces_;
    unordered_set<cmn::Address> peers_;

    Timepoint start_ = std::chrono::system_clock::now();
    Timepoint now_ = start_;
    double elapsed_ = 0;
    Timepoint last_report_ = start_;
    double bytes_downloaded_ = 0;

    ostream& log() {
        return std::cout << "[monitor] ";
    }
};

void monitor_thread(const TorrentContext& ctx, const unique_ptr<vector<Peer>>&& peers) {
    MonitorVisitor monitor{ctx};

    while (!monitor.complete()) {
        Result result;
        while (!ctx.result_queue->pop(result));
        std::visit(monitor, result);
    }
}
