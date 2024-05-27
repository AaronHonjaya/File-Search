/*
 * Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include "./HttpConnection.h"

#include <stdint.h>
#include <string.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"

#define READ_SIZE 1024
#define BUFF_SIZE 1024
#define SCALE_FACTOR 2

using std::map;
using std::string;
using std::vector;
using namespace boost;

namespace hw4 {

static const char* kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;

bool HttpConnection::GetNextRequest(HttpRequest* const request) {
  // Use WrappedRead from HttpUtils.cc to read bytes from the files into
  // private buffer_ variable. Keep reading until:
  // 1. The connection drops
  // 2. You see a "\r\n\r\n" indicating the end of the request header.
  //
  // Hint: Try and read in a large amount of bytes each time you call
  // WrappedRead.
  //
  // After reading complete request header, use ParseRequest() to parse into
  // an HttpRequest and save to the output parameter request.
  //
  // Important note: Clients may send back-to-back requests on the same socket.
  // This means WrappedRead may also end up reading more than one request.
  // Make sure to save anything you read after "\r\n\r\n" in buffer_ for the
  // next time the caller invokes GetNextRequest()!

  // STEP 1:

  char buff[BUFF_SIZE];
  while (1) {
    size_t end_index = buffer_.find(kHeaderEnd);
    if (end_index == std::string::npos) {
      int bytes_read =
          WrappedRead(fd_, reinterpret_cast<unsigned char*>(buff), BUFF_SIZE);
      if (bytes_read == -1) {
        std::cerr << "Error With Read" << std::endl;
        return false;
      }
      buffer_ += string(buff, bytes_read);

      if (bytes_read == 0) {
        if (buffer_.length() == 0) {
          return false;
        }
        *request = ParseRequest(buffer_);
        break;
      }
    } else {
      *request = ParseRequest(buffer_.substr(0, end_index));
      buffer_ = buffer_.substr(end_index + kHeaderEndLen);
      break;
    }
  }

  return true;
}

bool HttpConnection::WriteResponse(const HttpResponse& response) const {
  string str = response.GenerateResponseString();
  int res = WrappedWrite(
      fd_, reinterpret_cast<const unsigned char*>(str.c_str()), str.length());
  if (res != static_cast<int>(str.length())) return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string& request) const {
  HttpRequest req("/");  // by default, get "/".

  // Plan for STEP 2:
  // 1. Split the request into different lines (split on "\r\n").
  // 2. Extract the URI from the first line and store it in req.URI.
  // 3. For the rest of the lines in the request, track the header name and
  //    value and store them in req.headers_ (e.g. HttpRequest::AddHeader).
  //
  // Hint: Take a look at HttpRequest.h for details about the HTTP header
  // format that you need to parse.
  //
  // You'll probably want to look up boost functions for:
  // - Splitting a string into lines on a "\r\n" delimiter
  // - Trimming whitespace from the end of a string
  // - Converting a string to lowercase.
  //
  // Note: If a header is malformed, skip that line.

  // STEP 2:

  vector<string> lines;
  boost::split(lines, request, is_any_of("\r\n"), token_compress_on);
  vector<string> temp;
  boost::split(temp, lines.at(0), is_any_of(" "), token_compress_on);
  // std::cout << "request = " << request << std::endl;
  // std::cout << "size = " << lines.size() << std::endl;
  // std::cout << "first line = " << lines.at(0) << std::endl;
  req.set_uri(temp.at(1));

  for (uint64_t i = 1; i < lines.size(); i++) {
    to_lower(lines[i]);
    boost::split(temp, lines[i], is_any_of(":"), token_compress_on);
    if (temp.size() != 2) {
      continue;
    }
    trim(temp[0]);
    trim(temp[1]);
    req.AddHeader(temp[0], temp[1]);
  }
  return req;
}

}  // namespace hw4
