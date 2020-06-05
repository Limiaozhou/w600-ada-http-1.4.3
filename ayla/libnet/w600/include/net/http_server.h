 /**
  ******************************************************************************
  * @file    http_server.h
  * @author
  * @version
  * @brief   This file provides user interface for HTTP/HTTPS server.
  */
#ifndef _HTTPD_H_
#define _HTTPD_H_

#include <ada/server_req.h>

#define  HTTP_SERVER_TASK_SIZE  (1024+1024)
#define  HTTP_SERVER_TASK_PRIO	(62)  //tskIDLE_PRIORITY+1
#define  RECVBUF_SIZE           (512+256+128)
#define  MYPORT    80    // the port users will be connecting to
#define  BACKLOG   5     // how many pending connections queue will hold
#define  URI_LEN   (128+64)
#define  CRLF                "\r\n"
#define  HTTP_HEADER_END     "\r\n\r\n"
#define  HTTP_HEADER_URL     "/"
#define  HTTP_HEADER_HTTP    " HTTP"
#define  HTTP_CONTENT_LENGTH  "Content-Length"
#define  HTTP_SERVER_DEBUG    0
#define  HTTP_SERVER_TIMEOUT  10



struct http_state_s
{
    int sock;
    fd_set *fd;
    const char *file;     /* Pointer to first unsent byte in buf. */
    u32_t left;           /* Number of unsent bytes in buf. */

    u32_t  used;        // received header size.
    char   head[RECVBUF_SIZE];
    // if in post mode the receive buffer exceed our head buffer size,
    // we alloc a buffer for the body.
    u32_t  size;        // max size of the body buffer.
    u32_t  recv;        // received body data size.
    char*  body;        // point to head + used if head buffer is enough.
    
    char uri[URI_LEN];
};

/**
 * @brief     This function is used to start an HTTP or HTTPS server.
 */
 void httpd_init_ayla(void);

/**
 * @brief     This function is used to free memory allocated by httpd API, such as httpd_request_get_header_field() and httpd_request_get_query_key().
 * @param[in] ptr: pointer to memory to be deallocated
 * @return    None
 */
void httpd_free(void *ptr);

/**
 * @brief     This function is used to close a client connection and release context resource.
 */
void httpd_conn_close(struct http_state_s *hs);

/**
 * @brief     This function is used to check HTTP method of request in connection context.
 * @param[in] conn: pointer to connection context
 * @param[in] method: HTTP method string to compare with
 * @return    0 : if different
 * @return    1 : if matched
 */
int httpd_request_is_method(struct http_state_s *hs, char *method);

/**
 * @brief      This function is used to read data from HTTP/HTTPS connection.
 */
int httpd_request_read_data(struct http_state_s *hs, uint8_t *data, size_t data_len);

/**
 * @brief      This function is used to get a header field from HTTP header of connection context.
 * @param[in]  conn: pointer to connection context
 * @param[in]  field: header field string to search
 * @param[out] value: search result stored in memory allocated
 * @return    0 : if found
 * @return    -1 : if not found
 * @note      The search result memory should be free by httpd_free().
 */
 int httpd_request_get_header_field(struct http_state_s *hs, char *field, char **value);

/**
 * @brief      This function is used to write HTTP response body data to connection.
 */
 int httpd_response_write_data(struct http_state_s *hs, uint8_t *data, size_t data_len);

/**
 * @brief      This function is used to write a default HTTP response for error of 400 Bad Request.
 */
 void httpd_response_bad_request(struct http_state_s *hs, char *msg);

/**
 * @brief      This function is used to write a default HTTP response for error of 404 Not Found.
 */
 void httpd_response_not_found(struct http_state_s *conn, char *msg);

/**
 * @brief     This function is used to register a callback function for a Web page request handling.
 * @param[in] path: resource path for a page
 * @param[in] callback: callback function to handle the request to page
 * @return    0 : if successful
 * @return    -1 : if error occurred
 */
int httpd_reg_page_callback(char *path, void (*callback)(struct http_state_s *hs));

/**
 * @brief      This function is used to get a key value from query string in HTTP header of connection context.
 * @param[in]  conn: pointer to connection context
 * @param[in]  key: key name string to search
 * @param[out] value: search result stored in memory allocated
 * @return    0 : if found
 * @return    -1 : if not found
 * @note      The search result memory should be free by httpd_free().
 */
int httpd_request_get_query_key(struct http_state_s *hs, char *key, char **value);

/**
 * @brief      This function is used to get http server's status.
 * @return    0 : http server is not in service
 * @return    1 : http server is in service
 */
bool httpd_get_hs_status(void);

/**
 * @brief      This function is used to set http server's state.
 * @param[in]  status: status to set,1 to enable and 0 to disable
 */
void httpd_set_hs_status(bool status);


/*\@}*/

#endif /* _HTTPD_H_ */

