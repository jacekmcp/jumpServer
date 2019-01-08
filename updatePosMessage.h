//
// Created by jacek on 08.01.19.
//

#ifndef SERVER_UPDATEPOSMESSAGE_H
#define SERVER_UPDATEPOSMESSAGE_H

#include "clientPos.cpp"
#include <unordered_set>
#include <bits/unordered_set.h>

class updatePosMessage {
public:
    updatePosMessage(char* message);

private:
    int timestamp;
    std::unordered_set<clientPos*> clients;

public:
    int getTimestamp() const;

    void setTimestamp(int timestamp);

    const std::unordered_set<clientPos *> &getClients() const;

    void setClients(const std::unordered_set<clientPos *> &clients);

};


#endif //SERVER_UPDATEPOSMESSAGE_H
