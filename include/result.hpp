//
// Created by Eleanor McMurtry on 05.04.23.
//

#ifndef PICOTOR_RESULT_HPP
#define PICOTOR_RESULT_HPP

#include <torrent.hpp>
#include <variant>

using std::variant;
using cmn::Address;

struct ResultPieceComplete {
    CompletePiece piece;
};

struct ResultPeerConnected {
    Address addr;
};

struct ResultPeerDropped {
    Address addr;
};

typedef variant<ResultPieceComplete, ResultPeerConnected, ResultPeerDropped> Result;

#endif //PICOTOR_RESULT_HPP
