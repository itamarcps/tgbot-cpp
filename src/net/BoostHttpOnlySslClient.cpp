#include "tgbot/net/BoostHttpOnlySslClient.h"

#include <boost/asio/ssl.hpp>

#include <cstddef>
#include <vector>

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

namespace TgBot {

BoostHttpOnlySslClient::BoostHttpOnlySslClient() : _httpParser() {
}

BoostHttpOnlySslClient::~BoostHttpOnlySslClient() {
}

string BoostHttpOnlySslClient::makeRequest(const Url& url, const vector<HttpReqArg>& args) const {
    tcp::resolver resolver(_ioService);
    tcp::resolver::query query(url.host, "8081"); // Use port 8081 for HTTP, local server

    tcp::socket socket(_ioService);

    connect(socket, resolver.resolve(query));

    #ifdef TGBOT_DISABLE_NAGLES_ALGORITHM
    socket.set_option(tcp::no_delay(true));
    #endif //TGBOT_DISABLE_NAGLES_ALGORITHM
    #ifdef TGBOT_CHANGE_SOCKET_BUFFER_SIZE
    #if _WIN64 || __amd64__ || __x86_64__ || __MINGW64__ || __aarch64__ || __powerpc64__
    socket.set_option(socket_base::send_buffer_size(65536));
    socket.set_option(socket_base::receive_buffer_size(65536));
    #else //for 32-bit
    socket.set_option(socket_base::send_buffer_size(32768));
    socket.set_option(socket_base::receive_buffer_size(32768));
    #endif //Processor architecture
    #endif //TGBOT_CHANGE_SOCKET_BUFFER_SIZE

    string requestText = _httpParser.generateRequest(url, args, false);
    write(socket, buffer(requestText.c_str(), requestText.length()));

    fd_set fileDescriptorSet;
    struct timeval timeStruct;
    
    // set the timeout to 20 seconds
    timeStruct.tv_sec = _timeout;
    timeStruct.tv_usec = 0;
    FD_ZERO(&fileDescriptorSet);
    
    int nativeSocket = static_cast<int>(socket.native_handle());
    
    FD_SET(nativeSocket, &fileDescriptorSet);        
    select(nativeSocket + 1, &fileDescriptorSet, NULL, NULL, &timeStruct);
    
    if (!FD_ISSET(nativeSocket, &fileDescriptorSet)) { // timeout
        std::string sMsg("TIMEOUT on read client data. Client IP: ");
        sMsg.append(socket.remote_endpoint().address().to_string());
        _ioService.reset();
        
        throw std::exception();
    }      
    
    string response;

    #ifdef TGBOT_CHANGE_READ_BUFFER_SIZE
    #if _WIN64 || __amd64__ || __x86_64__ || __MINGW64__ || __aarch64__ || __powerpc64__
    char buff[65536];
    #else //for 32-bit
    char buff[32768];
    #endif //Processor architecture
    #else
    char buff[1024];
    #endif //TGBOT_CHANGE_READ_BUFFER_SIZE

    boost::system::error_code error;
    while (!error) {
        std::size_t bytes = read(socket, buffer(buff), error);
        response += string(buff, bytes);
    }

    return _httpParser.extractBody(response);
}


}
