# Std.Http

HTTP client and server — built on Std\Net and Std\String.

Provides Request/Response types, high-level client (get, post),
and a simple server (serve). All networking via io_uring.

## Types

### `type Method = GET | POST | PUT | DELETE | PATCH | HEAD | OPTIONS`

HTTP method.

### `type Request = Request {`

HTTP request with method, path, headers, and body.

### `type Response = Response {`

HTTP response with status code, raw headers string, and body.

## Functions

### `request`

```yona
request method path headers body = Request { method = method, path = path, headers = headers, body = body }
```

Create a request with defaults.

### `methodString`

```yona
methodString m = case m of
```

Format a Method as a string.

### `formatRequest`

```yona
formatRequest host req =
```

Format a Request into an HTTP/1.1 request string.

### `parseResponse`

```yona
parseResponse raw =
```

Parse an HTTP response string into a Response.

### `send`

```yona
send host port req =
```

Send an HTTP request to host:port and return the Response.

```
send "example.com" 80 (request GET "/" 0 "")
```

### `get`

```yona
get host port path = send host port (request GET path 0 "")
```

HTTP GET shorthand.

```
get "example.com" 80 "/api"
```

### `post`

```yona
post host port path body = send host port (request POST path 0 body)
```

HTTP POST shorthand.

```
post "example.com" 80 "/submit" "key=value"
```

### `ok`

```yona
ok body = Response { status = 200, rawHeaders = "", body = body }
```

Create a simple 200 OK response.

```
ok "Hello, World!"
```

### `notFound`

```yona
notFound = Response { status = 404, rawHeaders = "", body = "Not Found" }
```

Create a 404 Not Found response.

### `serverError`

```yona
serverError = Response { status = 500, rawHeaders = "", body = "Internal Server Error" }
```

Create a 500 Internal Server Error response.

### `formatResponse`

```yona
formatResponse resp =
```

Format a Response into an HTTP/1.1 response string.

### `response`

```yona
response status body = Response { status = status, rawHeaders = "", body = body }
```

Create a Response with custom status and body.

### `serve`

```yona
serve host port handler =
```

Start an HTTP server. Calls `handler request` for each incoming connection.
The handler receives a Request and returns a Response.
Runs forever (blocking).

```
serve "0.0.0.0" 8080 (\req -> ok "Hello!")
```

