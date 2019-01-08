//
// Created by jacek on 08.01.19.
//

#include <unordered_set>
#include "updatePosMessage.h"

int updatePosMessage::getTimestamp() const {
    return timestamp;
}

void updatePosMessage::setTimestamp(int timestamp) {
    updatePosMessage::timestamp = timestamp;
}

const std::unordered_set<clientPos *> &updatePosMessage::getClients() const {
    return clients;
}

void updatePosMessage::setClients(const std::unordered_set<clientPos *> &clients) {
    updatePosMessage::clients = clients;
}

updatePosMessage::updatePosMessage(char* message) {



}

