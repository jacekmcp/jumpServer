#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <unordered_set>
#include <signal.h>
#include <map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace std;

class Client;

int servFd;
int epollFd;

bool busy[4];

std::unordered_set<Client*> clients;

void ctrl_c(int);

uint16_t readPort(char * txt);

int getAvailableColor();

void setReuseAddr(int sock);

void sendPositionsToAll();

void fdToStr(int fd, stringstream& ss);

void intToStr(int num, stringstream& ss);

void killPlayer(int fd);

void sendToAll(char * buffer);

void sendToAll(stringstream& ss);

void sendToAllBut(int fd, char * buffer);

void disconnectClients();

struct Handler {
    virtual ~Handler(){}
    virtual void handleEvent(uint32_t events) = 0;
};

struct clientPos{
    int positionX;
    int positionY;
};



class Client : public Handler{
    int _fd;
    int _points;
    clientPos _position;
    int _timestamp;
    int _color;
private:
    void updateClientPos(char * message){
        stringstream posX;
        stringstream posY;
        stringstream ts;
        stringstream killed;

        for(int i = 0; i<10; i++){
            ts << message[i];;
        }

        for(int i = 10; i<13; i++){
            posX << message[i];;
        }

        for(int i =13; i<16; i++){
            posY << message[i];;
        }

        for(int i = 16; i<18; i++){
            killed << message[i];
        }

        int resTimeStamp = stoi(ts.str()); // tu sie wywala bez sprawdzania warunku count == 0

        if(resTimeStamp >= _timestamp){
            if(_position.positionX != stoi(posX.str()) or _position.positionY != stoi(posY.str())){
                _position.positionX = stoi(posX.str());
                _position.positionY = stoi(posY.str());
                _timestamp = resTimeStamp;

                int killer = stoi(killed.str());
                if(killer != 0){
                    _points++;
                    killPlayer(killer);
                }
                sendPositionsToAll();
            }
        }
    }

public:
    Client(int fd) : _fd(fd) {
        epoll_event ee {EPOLLIN|EPOLLRDHUP, {.ptr=this}};
        epoll_ctl(epollFd, EPOLL_CTL_ADD, _fd, &ee);
        _points = 0;
        _position.positionX = 100;// rand() % 640;
        _position.positionY = 100;// rand() % 480;
        _timestamp = time(NULL);
        _color = getAvailableColor();
        busy[_color] = true;
    }

    virtual ~Client(){
        epoll_ctl(epollFd, EPOLL_CTL_DEL, _fd, nullptr);
        shutdown(_fd, SHUT_RDWR);
        busy[_color] = false;
        close(_fd);
    }

    // getters
    int fd() const {return _fd;}
    int points() const {return _points;}
    const clientPos &get_position() const {
        return _position;
    }

    int get_color() const {
        return _color;
    }

    void set_color(int _color) {
        Client::_color = _color;
    }

    // setters
    void set_position(const clientPos &_position) {
        Client::_position = _position;
    }
    void points(int a) {_points = a;}

    virtual void handleEvent(uint32_t events) override {
        if(events & EPOLLIN) {
            read(events);
        }
        if(events & ~EPOLLIN){
            remove();
        }
    }

    void read(uint32_t events){
        char dataFromRead[18], data[9];

        ssize_t count  = ::read(_fd, dataFromRead, sizeof(dataFromRead)-1);
        if(count <= 0) events |= EPOLLERR;

        if(count == -1){
            return;
        }

        cout << "gracz "<< _fd <<"wysyła: " << dataFromRead << endl;

        if(count != 0){
            this->updateClientPos(dataFromRead);
        }

//        printf("client: %d \t READ: %s \n", _fd, dataFromRead);
    }

    void write(char * buffer){
        if ( ::write(_fd, buffer, strlen(buffer)) != (int) strlen(buffer) ) perror("write failed");
//        printf("client: %d \t WRITE: %s \n", _fd, buffer);
    }

    void write(stringstream& ss){
        if ( ::write(_fd, ss.str().c_str(), strlen(ss.str().c_str())) != (int) strlen(ss.str().c_str()) ) perror("write failed");
//        printf("client: %d \t WRITE: %s \n", _fd, ss.str().c_str());
    }

    void remove() {
        clients.erase(this);
        delete this;
        sendPositionsToAll();
    }
};



class : Handler {
public:
    virtual void handleEvent(uint32_t events) override {
        if(events & EPOLLIN){
            sockaddr_in clientAddr{};
            socklen_t clientAddrSize = sizeof(clientAddr);

            auto clientFd = accept(servFd, (sockaddr*) &clientAddr, &clientAddrSize);
            if(clientFd == -1) error(1, errno, "accept failed");

            printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);

            if(clients.size() >= 4){
                char message[2] = "0";
                ::write(clientFd, message, strlen(message));
            } else {
//                char tmp[2];
                stringstream ss;
                fdToStr(clientFd, ss);



//                sprintf(tmp, "%d", clientFd);
                Client *c = new Client(clientFd);

                ss<<c->get_color();
                ::write(clientFd, ss.str().c_str(), strlen(ss.str().c_str()));
                clients.insert(c);
                sendPositionsToAll();
            }
        }

        if(events & ~EPOLLIN){
            error(0, errno, "Event %x on server socket", events);
            ctrl_c(SIGINT);
        }
    }
} servHandler;


int main(int argc, char ** argv){
//    if(argc != 2) error(1, 0, "Need 1 arg (port)");
    auto port = readPort("4200");

    servFd = socket(AF_INET, SOCK_STREAM, 0);
    if(servFd == -1) error(1, errno, "socket failed");

    signal(SIGINT, ctrl_c);
    signal(SIGPIPE, SIG_IGN);

    setReuseAddr(servFd);

    sockaddr_in serverAddr{.sin_family=AF_INET, .sin_port=htons((short)port), .sin_addr={INADDR_ANY}};
    int res = bind(servFd, (sockaddr*) &serverAddr, sizeof(serverAddr));
    if(res) error(1, errno, "bind failed");

    res = listen(servFd, 1);
    if(res) error(1, errno, "listen failed");

    epollFd = epoll_create1(0);

    epoll_event ee {EPOLLIN, {.ptr=&servHandler}};
    epoll_ctl(epollFd, EPOLL_CTL_ADD, servFd, &ee);

    srand(time(NULL));

    for (bool &i : busy) {
        i = false;
    }

    while(true){
        if(-1 == epoll_wait(epollFd, &ee, 1, -1)) {
            error(0,errno,"epoll_wait failed");
            ctrl_c(SIGINT);
        }

        ((Handler*)ee.data.ptr)->handleEvent(ee.events);


        printf("\t ----------------- \n");
    }
}

// *************** SOCKETS ******************

uint16_t readPort(char * txt){
    char * ptr;
    auto port = strtol(txt, &ptr, 10);
    if(*ptr!=0 || port<1 || (port>((1<<16)-1))) error(1,0,"illegal argument %s", txt);
    return port;
}


void setReuseAddr(int sock){
    const int one = 1;
    int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if(res) error(1,errno, "setsockopt failed");
}


void ctrl_c(int){

    disconnectClients();

    for(Client * client : clients)
        delete client;

    close(servFd);
    printf(" \n Closing server \n");
    exit(0);
}


// *************** SEND TO CLIENTS ******************


void sendPositionsToAll(){
    stringstream ss;
    stringstream message;

    message<<time(NULL);

    int i = 0;
    auto it = clients.begin();

    bool localBusy[4] = {false, false, false, false};

    while(it!=clients.end()){
        Client * client = *it;

        it++;
        ss.str("");

        fdToStr(client->fd(), ss);
        fdToStr(client->points(), ss);
        intToStr(client->get_position().positionX, ss);
        intToStr(client->get_position().positionY, ss);

        message<<ss.str().c_str()<<client->get_color();
        localBusy[client->get_color()] = true;
        i++;
    }



    for(i; i<4; i++){
        for(int j = 0; j<4; j++) {
            if(!localBusy[j]){
                localBusy[j] = true;
                message<<"0000000000"<<j;
            }
        }


    }

    sendToAll(message);
}

void fdToStr(int fd, stringstream& ss){
    if(fd < 10)ss<<0<<fd;
    else ss<<fd;
}

void intToStr(int num, stringstream& ss){
    if(num < 100) ss<<0;
    if(num < 10)ss<<0;
    ss<<num;
}

void sendToAll(stringstream& ss){
    auto it = clients.begin();
    while(it!=clients.end()){
        Client * client = *it;
        it++;
        client->write(ss);
    }
}

void killPlayer(int fd){
    auto it = clients.begin();
    while(it!=clients.end()){
        Client * client = *it;
        it++;
        if(client->fd() == fd){
            client->points(0);

            clientPos clientPos1 = clientPos();
            clientPos1.positionX = rand() % 640;
            clientPos1.positionY = rand() % 480;
            client->set_position(clientPos1);
        }
    }
}

void disconnectClients(){
    auto it = clients.begin();
    char message[18] = {'0', '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};
    while(it!=clients.end()){
        Client * client = *it;
        it++;
        client->write(message);
    }
}

void sendToAll(char * buffer){
    auto it = clients.begin();
    while(it!=clients.end()){
        Client * client = *it;
        it++;
        client->write(buffer);
    }
}

int getAvailableColor(){
    for(int i=0; i<4; i++){
        if(!busy[i]) return i;
    }
}
