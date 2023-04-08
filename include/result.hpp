//
// Created by Eleanor McMurtry on 05.04.23.
//

#ifndef PICOTOR_RESULT_HPP
#define PICOTOR_RESULT_HPP

#include <torrent.hpp>
#include <variant>

struct ResultPieceComplete {
    CompletePiece piece;
};

struct ResultPeerConnected {
    cmn::Address addr;
};

struct ResultPeerDropped {
    cmn::Address addr;
};

typedef std::variant<ResultPieceComplete, ResultPeerConnected, ResultPeerDropped> Result;

#endif //PICOTOR_RESULT_HPP
