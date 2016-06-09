#include <iostream>
#include <vector>

using namespace std;

struct Peer {
    string ip;
    int port;
};

int main(int argc, char* argv[]) {
    if (argc != 1 && argc != 3) {
        cerr << "Invalid Input" << endl;
        return -1;
    }

    vector<Peer> peers;
    if (argc == 1) {

    } else {

    }
    return 0;
}
