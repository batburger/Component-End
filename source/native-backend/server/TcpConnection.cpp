//
// Created by albert on 3/12/18.
//

#include "native-backend/server/TcpConnection.h"
#include <native-backend/parsing/RequestInformation.h>
#include <native-backend/routing/Router.h>
#include <native-backend/errors/errors.h>
#include <string>
#include <native-backend/errors/HttpStatusCode.h>

/*!\brief Starts reading the request header asynchronously and calls TcpConnection::onRequestRead when done.
 * Called from Server::handle_accept when a client connects.
 * Expects connection to be present.*/
void nvb::TcpConnection::start() {
    boost::asio::async_read_until(socket_, this->request_buffer_, "\r\n\r\n",
                                  boost::bind(&TcpConnection::onRequestRead, this,
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
}

/*!\brief Creates a shared_ptr of TcpConnection and stores a copy of it in the created instance.
 * The copy is created to make sure that the shared_ptr isn't destroyed before asynchronous handling
 * of the request has finished.*/
nvb::TcpConnection::shared_ptr_t nvb::TcpConnection::create(boost::asio::io_context &io_context) {
    shared_ptr_t myPtr = shared_ptr_t(new TcpConnection(io_context));
    myPtr->setRef(myPtr);
    return myPtr;
}

/*!\brief Sets \c TcpConnection::shared_ptr_to_this_*/
void nvb::TcpConnection::setRef(nvb::TcpConnection::shared_ptr_t shared_ptr_to_this) {
    this->shared_ptr_to_this_ = std::move(shared_ptr_to_this);
}

/*!\brief Returns \c TcpConnection::socket_*/
tcp::socket &nvb::TcpConnection::getSocket() {
    return this->socket_;
}

nvb::TcpConnection::TcpConnection(boost::asio::io_context &io_context)
        : socket_(io_context) {}

/*!\brief Resets \c shared_ptr_to_this_ so it can be freed.
 * Called after the response has been sent to the client.*/
void nvb::TcpConnection::onResponded(const boost::system::error_code &, size_t) {
    shared_ptr_to_this_.reset();
}

/*!\brief Sends back the HTML code generated by TcpConnection::createResponse.
 * Called after the request has been read asynchronously.
 * Binds TcpConnection::onResponded for when the response has been written.*/
void nvb::TcpConnection::onRequestRead(const boost::system::error_code &, size_t) {
    std::string request((std::istreambuf_iterator<char>(&this->request_buffer_)),
                        std::istreambuf_iterator<char>());


    std::string message = createResponse(request);
    boost::asio::async_write(socket_, boost::asio::buffer(message),
                             boost::bind(&TcpConnection::onResponded, shared_from_this(),
                                         boost::asio::placeholders::error,
                                         boost::asio::placeholders::bytes_transferred));

}

/*!\brief Parses the request to create the response string.
 * The request gets passed to the Routs singleton*/
std::string nvb::TcpConnection::createResponse(std::string request) {
    if (request == "TEST TEST TEST\n") {
        return "ONLINE\n";
    }
    boost::shared_ptr<nvb::HtmlPackage> htmlPackage;
    std::string html;
    std::string js;
    StatusWrapper status = HttpStatusCode::status200;

    try {
        try {

            auto requestInformation = nvb::RequestInformation::create(request);
            htmlPackage = nvb::Router::getInstance()->evaluateRoute(requestInformation);
        }
        catch (nvb::GeneralError &e) {
            htmlPackage = boost::movelib::unique_ptr<nvb::HtmlPackage>(nullptr);
            html = "<p>A error occurred</p><br/><b>" + std::string(e.what()) + "</b> <br/> <b>Status Code: " +
                   std::to_string(e.statusCode()) + " " + e.statusText() + "</b>";

            status = e.status();
        }
    } catch (...) {
        htmlPackage = boost::movelib::unique_ptr<nvb::HtmlPackage>(nullptr);
        html = "<p>A error occurred</p><br/> <b>No further details can be provided.</b> <br/> <b>Status Code: 500 Internal Server Error</b>";
        status = HttpStatusCode::status500;
    }

    try {
        try {
            if (htmlPackage.get() != nullptr) {
                html = htmlPackage->get();
            }
        } catch (nvb::GeneralError &e) {
            html = "<p>A error occurred</p><br/><b>" + std::string(e.what()) + "</b> <br/> <b>Status Code: " +
                   std::to_string(e.statusCode()) + " " + e.statusText() + "</b>";

            status = e.status();
        }
    } catch (...) {
        html = "<p>A error occurred</p><br/> <b>No further details can be provided.</b> <br/> <b>Status Code: 500 Internal Server Error</b>";
        status = HttpStatusCode::status500;
    }

    html = "<html>" + html + "</html>";

    std::string message = "HTTP/1.1 " + std::to_string(status.getCode()) + " " + status.getText() + "\n" +
                          "Content-length: " + std::to_string(html.size()) + "\n" +
                          "Content-Type: text/html\n\n" + html + "\n";

    return message;
}
