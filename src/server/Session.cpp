#include "Session.hpp"
#include <iostream>

Session::Session()
{
}

Session::Session(int socket_id): request(Request(socket_id))
{
    client_id = socket_id;
    status = S_NEW;
	std::time_t now = std::time(0);

    _client_socket = socket_id;
    std::srand(now);
	std::memset(_sessionId, 0, 100);
    std::string id = "session_id=" + utils::toString(rand() % 10000) + "_cookie";
    std::memmove(_sessionId, id.c_str(), id.size());
}

const char* Session::getSessionId() const
{
    return _sessionId;
}

Session::Session(const Session &src)
{
    request = src.request;
    response = src.response;

    std::time_t now = std::time(0);
    std::srand(now);
    std::memset(_sessionId, 0, 100);
    std::string id = "session_id=" + utils::toString(rand() % 10000) + "_cookie";
    std::memmove(_sessionId, id.c_str(), id.size());

    _client_socket = src._client_socket;
    client_id = src.client_id;
}

Session &Session::operator=(const Session &src)
{
    request = src.request;
    response = src.response;

    std::time_t now = std::time(0);
    std::srand(now);
    std::memset(_sessionId, 0, 100);
    std::string id = "session_id=" + utils::toString(rand() % 10000) + "_cookie";
    std::memmove(_sessionId, id.c_str(), id.size());

    _client_socket = src._client_socket;
    client_id = src.client_id;
    return *this;
}

void Session::recieveData()
{
    if (status != S_NEW && status != S_DONE)
    {
        request = Request(request, 1024 * 1024);
    }
    else
    {
        request.total_read = 0;
    }
    ssize_t bytes_read = recv(request.getSocket(), (void *)(request.buffer + request.total_read), request.getBufferLen() - request.total_read, 0);
    if (bytes_read == 0)
        throw Request::SocketCloseException("connection closed by client");
    else if (bytes_read < 0)
        throw Request::ParsingErrorException(Request::BAD_REQUEST, "malformed request");
    request.setBufferLen(bytes_read);
}

void Session::newRequest(std::vector<ServerConfig> &configs)
{
    request.parseHeaders();
    if (!request.setConfig(configs))
        throw Request::ParsingErrorException(Request::BAD_REQUEST, "hostname is not configured");
    response.setConfig(*request.getConfig());
    status = S_REQUEST;
    /* Check content-length */
    std::string content_length = request.getHeader("Content-Length");
    int contentLength = atoi(content_length.c_str());
    if (contentLength > request.getRouteConfig()->body_limit)
        throw Request::ParsingErrorException(Request::CONTENT_LENGTH, "content length is above the limit");
    if (contentLength == 0)
        status = S_PROCESS;
    response.setStatus(200);
}

void Session::parseBody()
{
    if (request.getRouteConfig()->body_limit < request.total_read)
        throw Request::ParsingErrorException(Request::CONTENT_LENGTH, "content length is above the limit");
}

int Session::getSocket() const
{
    return _client_socket;
}

void Session::sendResponse()
{
    ssize_t bytes_sent = send(getSocket(), response.getContent(), response.getContentLength(), MSG_DONTWAIT);

    response.setContent(bytes_sent);
}

void Session::handleCGI(const std::string& script_path) {
    try {
        CGIHandler cgi(script_path, request);
        std::string cgi_output = cgi.execute();
        response.setContent(cgi_output);
    } catch (const std::exception& e) {
        std::string error_msg = e.what();
        if (request.getConfig() != NULL) {
            // Use custom error page if configuration is available
            createErrorResponse(error_msg, request.getConfig());
        } else {
            // Basic error response if no configuration is available
            std::string basic_error = "HTTP/1.1 500 Internal Server Error\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "Connection: close\r\n"
                                    "\r\n"
                                    "500 Internal Server Error: CGI Processing Failed";
            response.setContent(basic_error);
        }
    }
}

Response& Session::createErrorResponse(const std::string& error_msg, const ServerConfig* config) {
    // Extract error code from error message (assuming format "500 CGI Error: ...")
    int error_code = 500; // Default error code
    size_t pos = error_msg.find(" ");
    if (pos != std::string::npos) {
        error_code = std::atoi(error_msg.substr(0, pos).c_str());
    }

    response.setStatus(error_code);

    // Use custom error page if available
    if (config && config->error_pages.find(error_code) != config->error_pages.end()) {
        std::string error_page_path = config->error_pages[error_code];
        // Read and set error page content
        // You might want to implement this based on your needs
    } else {
        // Basic error response
        response.setContent("HTTP/1.1 " + utils::toString(error_code) + " Error\r\n"
                          "Content-Type: text/plain\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          + error_msg);
    }

    return response;
}
