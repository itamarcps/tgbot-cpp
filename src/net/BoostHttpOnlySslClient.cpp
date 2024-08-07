#include "tgbot/net/BoostHttpOnlySslClient.h"

#include <boost/asio/ssl.hpp>

#include <cstddef>
#include <vector>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>


using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

namespace TgBot {

BoostHttpOnlySslClient::BoostHttpOnlySslClient() : _httpParser() {
}

BoostHttpOnlySslClient::~BoostHttpOnlySslClient() {
}

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>

using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

string BoostHttpOnlySslClient::makeRequest(const Url& url, const vector<HttpReqArg>& args) const {
    tcp::resolver resolver(_ioService);
    tcp::resolver::query query(url.host, "8081"); // Use port 8081 for HTTP, local server

    tcp::socket socket(_ioService);

    connect(socket, resolver.resolve(query));
    socket.set_option(tcp::no_delay(true));

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

    // Read the response
    boost::asio::streambuf responseBuffer;
    std::ostringstream responseStream;
    boost::system::error_code error;

    // Read until the end of the HTTP headers
    std::cout << "Reading response headers" << std::endl;
    boost::asio::read_until(socket, responseBuffer, "\r\n\r\n", error);
    if (error && error != boost::asio::error::eof) {
        throw boost::system::system_error(error); // Some other error
    }
    std::cout << "Reading response headers done: " << responseBuffer.size() << std::endl;

    // Extract the headers
    std::istream responseStreambuf(&responseBuffer);
    string header;
    size_t contentLength = 0;
    std::cout << "Extracting headers" << std::endl;
    while (std::getline(responseStreambuf, header) && header != "\r") {
        responseStream << header << "\n";
        if (header.find("Content-Length:") != std::string::npos) {
            contentLength = std::stoul(header.substr(header.find(":") + 1));
        }
    }
    std::cout << "Extracting headers done" << std::endl;
    responseStream << "\r\n";
    std::cout << "Response headers: " << responseStream.str() << std::endl;
    std::cout << "Response headers size: " << responseStream.str().size() << std::endl;

    uint64_t alreadyReceivedSize = responseBuffer.size();
    uint64_t missingLength = alreadyReceivedSize - contentLength;
    
    responseStream << &responseBuffer;
    responseBuffer.consume(alreadyReceivedSize);

    // Read the remaining part of the response based on Content-Length
    std::cout << "Reading response body" << std::endl;
    if (contentLength > 0) {
        while (missingLength > 0) {
            std::cout << "Reading response body with missing length: " << missingLength << std::endl;
            auto bytesReaded = boost::asio::read(socket, responseBuffer, error);
            missingLength -= bytesReaded;
            std::cout << "Reading response body with content length done" << std::endl;
            if (error == boost::asio::error::eof)
                break; // Connection closed cleanly by peer.
            else if (error)
                break; // Some other error.
        }
    } else {
        std::cout << "Reading response body with unknown content length" << std::endl;
        while (boost::asio::read(socket, responseBuffer, boost::asio::transfer_at_least(1), error)) {
            std::cout << "Reading response body with unknown content length done" << std::endl;
            if (error == boost::asio::error::eof)
                break; // Connection closed cleanly by peer.
            else if (error)
                break; // Some other error.
        }
    }

    // Append the body to the response
    responseStream << &responseBuffer;

    return _httpParser.extractBody(responseStream.str());
}

}
