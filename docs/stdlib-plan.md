
# Yona Standard Library -- Comprehensive Design

## Foundational Design Decisions

Before diving into individual modules, I need to establish cross-cutting conventions derived from the existing codebase.

**Conventions observed from the existing code:**
- Module declaration: `module Std\Foo exports f1, f2, TypeCtor1, TypeCtor2 as ... end`
- ADT syntax: `type Result a e = Ok a | Err e`
- Named fields: `type Person = Person { name : String, age : Int }`
- Functions use juxtaposition application: `f x y` not `f(x, y)` (both work but juxtaposition is idiomatic)
- Pipe operator `|>` for composition
- `extern async` for I/O functions; `extern` for sync C FFI
- Symbols (`:ok`, `:error`) for lightweight tags
- `case ... of ... end` for pattern matching
- `do ... end` for sequencing side effects
- Collections: `Seq` (lists `[1,2,3]`), `Set` (`{1,2,3}`), `Dict` (`{k: v}`)
- Types: `Int` (i64), `Float` (f64), `Bool`, `String`, `Byte`, `Char`, `Symbol`, `Unit`

**Key type mapping for the stdlib design:**
- Sync pure functions: regular `extern` or Yona-implemented
- All I/O: `extern async` -- runs on thread pool, returns `Promise<T>`, auto-awaited
- Errors: Return `Result a e` for expected failures; `raise` only for bugs/panics
- Opaque handles (files, sockets, etc.): Represented as `Int` (the runtime passes raw `i64` handles)

---

## Std\Result (existing -- enhanced)

The existing `Result` module is a foundation. I propose additions for the expanded stdlib.

```yona
module Std\Result exports
  Ok, Err,
  isOk, isErr, unwrapOr, map, mapErr,
  flatMap, flatten, toOption, fromOption,
  mapBoth, fold, getOk, getErr,
  collect, sequence
as

type Result a e = Ok a | Err e

# Existing
isOk r = case r of Ok _ -> true; Err _ -> false end
isErr r = case r of Err _ -> true; Ok _ -> false end
unwrapOr default r = case r of Ok v -> v; Err _ -> default end
map fn r = case r of Ok v -> Ok (fn v); Err e -> Err e end
mapErr fn r = case r of Err e -> Err (fn e); Ok v -> Ok v end

# New additions
flatMap fn r = case r of Ok v -> fn v; Err e -> Err e end
flatten r = case r of Ok (Ok v) -> Ok v; Ok (Err e) -> Err e; Err e -> Err e end
toOption r = case r of Ok v -> Some v; Err _ -> None end
fromOption err opt = case opt of Some v -> Ok v; None -> Err err end
mapBoth okFn errFn r = case r of Ok v -> Ok (okFn v); Err e -> Err (errFn e) end
fold okFn errFn r = case r of Ok v -> okFn v; Err e -> errFn e end
getOk r = case r of Ok v -> Some v; Err _ -> None end
getErr r = case r of Err e -> Some e; Ok _ -> None end

# Collect a Seq of Results into a Result of Seq.
# Returns Err on first error found.
collect results =
  case results of
    [] -> Ok []
    [Ok v | rest] ->
      case collect rest of
        Ok vs -> Ok (v :: vs)
        Err e -> Err e
      end
    [Err e | _] -> Err e
  end

# sequence is an alias for collect
sequence results = collect results

end
```

---

## Std\Option (existing -- enhanced)

```yona
module Std\Option exports
  Some, None,
  isSome, isNone, unwrapOr, map,
  flatMap, flatten, toResult, filter,
  getOrElse, orElse, zip, unzip
as

type Option a = Some a | None

# Existing
isSome opt = case opt of Some _ -> true; None -> false end
isNone opt = case opt of None -> true; Some _ -> false end
unwrapOr default opt = case opt of Some v -> v; None -> default end
map fn opt = case opt of Some v -> Some (fn v); None -> None end

# New
flatMap fn opt = case opt of Some v -> fn v; None -> None end
flatten opt = case opt of Some (Some v) -> Some v; _ -> None end
toResult err opt = case opt of Some v -> Ok v; None -> Err err end
filter pred opt = case opt of Some v -> if pred v then Some v else None; None -> None end
getOrElse thunk opt = case opt of Some v -> v; None -> thunk () end
orElse thunk opt = case opt of Some v -> Some v; None -> thunk () end
zip a b = case (a, b) of (Some x, Some y) -> Some (x, y); _ -> None end
unzip opt = case opt of Some (a, b) -> (Some a, Some b); None -> (None, None) end

end
```

---

## Std\String

This module is split between pure Yona functions and `extern` C shims. Most string operations are pure and synchronous.

```yona
module Std\String exports
  # Query
  length, isEmpty, charAt, indexOf, lastIndexOf,
  contains, startsWith, endsWith,
  # Transform
  toUpperCase, toLowerCase, trim, trimStart, trimEnd,
  reverse, repeat, padStart, padEnd,
  replace, replaceAll,
  # Decompose / Compose
  split, join, chars, fromChars, bytes, fromBytes,
  substring, slice, take, drop, takeWhile, dropWhile,
  # Conversion
  toInt, toFloat, fromInt, fromFloat,
  # Builder
  builder, append, build,
  # Encoding
  encodeUtf8, decodeUtf8,
  # Formatting
  format, interpolate
as

type StringError = InvalidIndex Int | InvalidUtf8 | ParseError String

# -- Query (all sync, extern C shims) --
# length : String -> Int
extern yona_Std_String__length : String -> Int

# isEmpty : String -> Bool
isEmpty s = length s == 0

# charAt : Int -> String -> Result Int StringError
# Returns character code at index, or Err InvalidIndex
extern yona_Std_String__charAt : Int -> String -> Int
# Wrapped to return Result:
charAt idx s =
  let len = length s in
  if idx < 0 || idx >= len then Err (InvalidIndex idx)
  else Ok (yona_Std_String__charAt idx s)

# indexOf : String -> String -> Int
# Returns index of first occurrence, -1 if not found
extern yona_Std_String__indexOf : String -> String -> Int

# lastIndexOf : String -> String -> Int
extern yona_Std_String__lastIndexOf : String -> String -> Int

# contains : String -> String -> Bool
contains needle haystack = indexOf needle haystack >= 0

# startsWith : String -> String -> Bool
extern yona_Std_String__startsWith : String -> String -> Bool

# endsWith : String -> String -> Bool
extern yona_Std_String__endsWith : String -> String -> Bool

# -- Transform (sync) --
extern yona_Std_String__toUpperCase : String -> String
extern yona_Std_String__toLowerCase : String -> String
extern yona_Std_String__trim : String -> String
extern yona_Std_String__trimStart : String -> String
extern yona_Std_String__trimEnd : String -> String
extern yona_Std_String__reverse : String -> String
extern yona_Std_String__repeat : Int -> String -> String

# padStart : Int -> String -> String -> String
# padStart targetLen padChar str
extern yona_Std_String__padStart : Int -> String -> String -> String

# padEnd : Int -> String -> String -> String
extern yona_Std_String__padEnd : Int -> String -> String -> String

# replace : String -> String -> String -> String
# replace needle replacement haystack
extern yona_Std_String__replace : String -> String -> String -> String

# replaceAll : String -> String -> String -> String
extern yona_Std_String__replaceAll : String -> String -> String -> String

# -- Decompose / Compose (sync) --

# split : String -> String -> Seq
# split delimiter str -> list of substrings
extern yona_Std_String__split : String -> String -> Seq

# join : String -> Seq -> String
# join separator parts -> concatenated string
extern yona_Std_String__join : String -> Seq -> String

# chars : String -> Seq
# Returns sequence of character codes (Int)
extern yona_Std_String__chars : String -> Seq

# fromChars : Seq -> String
extern yona_Std_String__fromChars : Seq -> String

# bytes : String -> Seq
# UTF-8 byte sequence
extern yona_Std_String__bytes : String -> Seq

# fromBytes : Seq -> Result String StringError
extern yona_Std_String__fromBytes : Seq -> String

# substring : Int -> Int -> String -> String
# substring start length str
extern yona_Std_String__substring : Int -> Int -> String -> String

# slice : Int -> Int -> String -> String
# slice start end str (supports negative indices)
extern yona_Std_String__slice : Int -> Int -> String -> String

# take : Int -> String -> String
take n s = substring 0 n s

# drop : Int -> String -> String
drop n s = substring n (length s - n) s

# takeWhile : (Int -> Bool) -> String -> String
# Takes chars while predicate holds (operates on char codes)
takeWhile pred s =
  let cs = chars s in
  let go acc remaining =
    case remaining of
      [] -> acc
      [c | rest] -> if pred c then go (acc ++ [c]) rest else acc
    end
  in fromChars (go [] cs)

# dropWhile : (Int -> Bool) -> String -> String
dropWhile pred s =
  let cs = chars s in
  let go remaining =
    case remaining of
      [] -> []
      [c | rest] -> if pred c then go rest else [c | rest]
    end
  in fromChars (go cs)

# -- Conversion (sync) --
extern yona_Std_String__toInt : String -> Int
extern yona_Std_String__toFloat : String -> Float
extern yona_Std_String__fromInt : Int -> String
extern yona_Std_String__fromFloat : Float -> String

# Safe versions returning Result
toInt s = try Ok (yona_Std_String__toInt s) catch _ -> Err (ParseError s) end
toFloat s = try Ok (yona_Std_String__toFloat s) catch _ -> Err (ParseError s) end
fromInt n = yona_Std_String__fromInt n
fromFloat f = yona_Std_String__fromFloat f

# -- Builder (sync, for efficient string concatenation) --
# Builder is represented as a Seq of String fragments
# builder : Unit -> Seq
builder () = []

# append : String -> Seq -> Seq
append s b = b ++ [s]

# build : Seq -> String
build fragments = join "" fragments

# -- Encoding --
# encodeUtf8 : String -> Seq  (String -> byte sequence)
encodeUtf8 s = bytes s

# decodeUtf8 : Seq -> Result String StringError
decodeUtf8 bs = try Ok (fromBytes bs) catch _ -> Err InvalidUtf8 end

# -- Formatting --
# format : String -> Seq -> String
# Positional: format "Hello {0}, you are {1}" ["world", "cool"]
extern yona_Std_String__format : String -> Seq -> String

end
```

---

## Std\IO

```yona
module Std\IO exports
  # Console
  print, println, readLine, eprint, eprintln,
  # Streams
  StreamIn, StreamOut, StreamErr,
  stdin, stdout, stderr,
  streamWrite, streamWriteLine, streamRead, streamReadLine,
  streamFlush, streamClose,
  # Buffered
  BufferedReader, BufferedWriter,
  bufferedReader, bufferedWriter,
  readLineBuffered, writeBuffered, flushBuffered
as

type IOError = IOError { message : String, code : Int }
type Stream = StreamIn Int | StreamOut Int | StreamErr Int

# -- Console I/O (all async) --

# print : String -> Unit
extern async yona_Std_IO__print : String -> Unit

# println : String -> Unit
extern async yona_Std_IO__println : String -> Unit

# readLine : Unit -> String
extern async yona_Std_IO__readLine : Unit -> String

# eprint : String -> Unit
extern async yona_Std_IO__eprint : String -> Unit

# eprintln : String -> Unit
extern async yona_Std_IO__eprintln : String -> Unit

# -- Standard Streams --
# These return opaque stream handles (Int)

# stdin : Stream
stdin = StreamIn 0

# stdout : Stream
stdout = StreamOut 1

# stderr : Stream
stderr = StreamErr 2

# streamWrite : String -> Stream -> Result Unit IOError
extern async yona_Std_IO__streamWrite : String -> Int -> Int

# streamWriteLine : String -> Stream -> Result Unit IOError
streamWriteLine s stream = streamWrite (s ++ "\n") stream

# streamRead : Int -> Stream -> Result String IOError
# Read up to n bytes from stream
extern async yona_Std_IO__streamRead : Int -> Int -> String

# streamReadLine : Stream -> Result String IOError
extern async yona_Std_IO__streamReadLine : Int -> String

# streamFlush : Stream -> Result Unit IOError
extern async yona_Std_IO__streamFlush : Int -> Unit

# streamClose : Stream -> Result Unit IOError
extern async yona_Std_IO__streamClose : Int -> Unit

# -- Buffered I/O --
# BufferedReader/Writer wrap a stream handle with a buffer size

type BufferedReader = BufferedReader { handle : Int, bufSize : Int }
type BufferedWriter = BufferedWriter { handle : Int, bufSize : Int }

# bufferedReader : Int -> Stream -> BufferedReader
# bufferedReader bufferSize stream
bufferedReader size stream =
  case stream of
    StreamIn h -> BufferedReader { handle = h, bufSize = size }
    _ -> raise (:io_error, "Cannot create reader from output stream")
  end

# bufferedWriter : Int -> Stream -> BufferedWriter
bufferedWriter size stream =
  case stream of
    StreamOut h -> BufferedWriter { handle = h, bufSize = size }
    StreamErr h -> BufferedWriter { handle = h, bufSize = size }
    _ -> raise (:io_error, "Cannot create writer from input stream")
  end

# readLineBuffered : BufferedReader -> Result String IOError
extern async yona_Std_IO__readLineBuffered : Int -> Int -> String

# writeBuffered : String -> BufferedWriter -> Result Unit IOError
extern async yona_Std_IO__writeBuffered : String -> Int -> Int -> Unit

# flushBuffered : BufferedWriter -> Result Unit IOError
extern async yona_Std_IO__flushBuffered : Int -> Unit

end
```

---

## Std\File

```yona
module Std\File exports
  # Reading
  readFile, readBytes, readLines,
  # Writing
  writeFile, writeBytes, appendFile,
  # Streaming
  FileHandle, open, close, read, write, seek, tell, flush,
  OpenMode, ReadOnly, WriteOnly, ReadWrite, Append,
  # Queries
  exists, isFile, isDir, isSymlink,
  size, modifiedTime, createdTime,
  # Directory
  listDir, listDirRecursive, mkdir, mkdirp, rmdir,
  # Manipulation
  remove, rename, copy, symlink, readLink,
  # Path operations
  joinPath, dirName, baseName, extension, withExtension,
  absolutePath, relativePath, normalize,
  pathSeparator, isAbsolute, isRelative,
  # Temp
  tempFile, tempDir,
  # Watch
  watch, FileEvent, Created, Modified, Deleted,
  # Permissions
  permissions, setPermissions,
  # Error
  FileError, NotFound, PermissionDenied, AlreadyExists, IsDirectory, NotDirectory
as

type FileError
  = NotFound String
  | PermissionDenied String
  | AlreadyExists String
  | IsDirectory String
  | NotDirectory String
  | IOError { message : String, path : String }

type OpenMode = ReadOnly | WriteOnly | ReadWrite | Append

type FileHandle = FileHandle { fd : Int, path : String, mode : OpenMode }

type FileEvent = Created String | Modified String | Deleted String

type FileStat = FileStat {
  path : String,
  size : Int,
  isFile : Bool,
  isDir : Bool,
  isSymlink : Bool,
  modifiedTime : Int,
  createdTime : Int,
  permissions : Int
}

# -- Read entire file (async) --

# readFile : String -> Result String FileError
extern async yona_Std_File__readFile : String -> String
readFile path = try Ok (yona_Std_File__readFile path)
  catch _ -> Err (NotFound path) end

# readBytes : String -> Result Seq FileError
extern async yona_Std_File__readBytes : String -> Seq

# readLines : String -> Result Seq FileError
# Returns Seq of String (one per line)
readLines path =
  case readFile path of
    Ok content -> Ok (split "\n" content)
    Err e -> Err e
  end

# -- Write entire file (async) --

# writeFile : String -> String -> Result Unit FileError
extern async yona_Std_File__writeFile : String -> String -> Unit

# writeBytes : String -> Seq -> Result Unit FileError
extern async yona_Std_File__writeBytes : String -> Seq -> Unit

# appendFile : String -> String -> Result Unit FileError
extern async yona_Std_File__appendFile : String -> String -> Unit

# -- Streaming file I/O (async) --

# open : String -> OpenMode -> Result FileHandle FileError
extern async yona_Std_File__open : String -> Int -> Int

# close : FileHandle -> Result Unit FileError
extern async yona_Std_File__close : Int -> Unit

# read : Int -> FileHandle -> Result String FileError
# Read up to n bytes
extern async yona_Std_File__read : Int -> Int -> String

# write : String -> FileHandle -> Result Unit FileError
extern async yona_Std_File__write : String -> Int -> Unit

# seek : Int -> FileHandle -> Result Unit FileError
extern async yona_Std_File__seek : Int -> Int -> Unit

# tell : FileHandle -> Result Int FileError
extern async yona_Std_File__tell : Int -> Int

# flush : FileHandle -> Result Unit FileError
extern async yona_Std_File__flush : Int -> Unit

# -- File queries (async) --

# exists : String -> Bool
extern async yona_Std_File__exists : String -> Bool

# isFile : String -> Bool
extern async yona_Std_File__isFile : String -> Bool

# isDir : String -> Bool
extern async yona_Std_File__isDir : String -> Bool

# isSymlink : String -> Bool
extern async yona_Std_File__isSymlink : String -> Bool

# size : String -> Result Int FileError
extern async yona_Std_File__size : String -> Int

# modifiedTime : String -> Result Int FileError
extern async yona_Std_File__modifiedTime : String -> Int

# createdTime : String -> Result Int FileError
extern async yona_Std_File__createdTime : String -> Int

# -- Directory operations (async) --

# listDir : String -> Result Seq FileError
extern async yona_Std_File__listDir : String -> Seq

# listDirRecursive : String -> Result Seq FileError
extern async yona_Std_File__listDirRecursive : String -> Seq

# mkdir : String -> Result Unit FileError
extern async yona_Std_File__mkdir : String -> Unit

# mkdirp : String -> Result Unit FileError
# Create directory and all parents
extern async yona_Std_File__mkdirp : String -> Unit

# rmdir : String -> Result Unit FileError
extern async yona_Std_File__rmdir : String -> Unit

# -- File manipulation (async) --

# remove : String -> Result Unit FileError
extern async yona_Std_File__remove : String -> Unit

# rename : String -> String -> Result Unit FileError
extern async yona_Std_File__rename : String -> String -> Unit

# copy : String -> String -> Result Unit FileError
extern async yona_Std_File__copy : String -> String -> Unit

# symlink : String -> String -> Result Unit FileError
extern async yona_Std_File__symlink : String -> String -> Unit

# readLink : String -> Result String FileError
extern async yona_Std_File__readLink : String -> String

# -- Path operations (sync, pure) --

# joinPath : String -> String -> String
extern yona_Std_File__joinPath : String -> String -> String

# dirName : String -> String
extern yona_Std_File__dirName : String -> String

# baseName : String -> String
extern yona_Std_File__baseName : String -> String

# extension : String -> String
extern yona_Std_File__extension : String -> String

# withExtension : String -> String -> String
extern yona_Std_File__withExtension : String -> String -> String

# absolutePath : String -> Result String FileError
extern async yona_Std_File__absolutePath : String -> String

# relativePath : String -> String -> String
extern yona_Std_File__relativePath : String -> String -> String

# normalize : String -> String
extern yona_Std_File__normalize : String -> String

# pathSeparator : String
pathSeparator = "/"

# isAbsolute : String -> Bool
isAbsolute p = startsWith "/" p

# isRelative : String -> Bool
isRelative p = !(isAbsolute p)

# -- Temp files (async) --

# tempFile : String -> Result (String, FileHandle) FileError
# tempFile prefix -> (path, handle)
extern async yona_Std_File__tempFile : String -> Int

# tempDir : String -> Result String FileError
# tempDir prefix -> path
extern async yona_Std_File__tempDir : String -> String

# -- Watch (async) --
# watch : String -> (FileEvent -> Unit) -> Result Unit FileError
# Watches path for changes, calls callback on events
extern async yona_Std_File__watch : String -> Int -> Unit

# -- Permissions (async) --
# permissions : String -> Result Int FileError  (unix mode bits)
extern async yona_Std_File__permissions : String -> Int

# setPermissions : String -> Int -> Result Unit FileError
extern async yona_Std_File__setPermissions : String -> Int -> Unit

end
```

---

## Std\Net

```yona
module Std\Net exports
  # TCP
  TcpListener, TcpStream,
  tcpListen, tcpAccept, tcpConnect,
  tcpRead, tcpWrite, tcpClose,
  tcpSetTimeout, tcpLocalAddr, tcpRemoteAddr,
  # UDP
  UdpSocket,
  udpBind, udpSendTo, udpRecvFrom, udpClose,
  # DNS
  resolve, resolveAll,
  # Address
  SocketAddr, ipv4, ipv6, addr, port,
  # Errors
  NetError, ConnectionRefused, ConnectionReset, TimedOut,
  AddrInUse, AddrNotAvailable, DnsError
as

type NetError
  = ConnectionRefused String
  | ConnectionReset String
  | TimedOut String
  | AddrInUse String
  | AddrNotAvailable String
  | DnsError String
  | NetIOError { message : String, code : Int }

type SocketAddr = SocketAddr { host : String, port : Int }

type TcpListener = TcpListener { fd : Int, addr : SocketAddr }
type TcpStream = TcpStream { fd : Int, localAddr : SocketAddr, remoteAddr : SocketAddr }
type UdpSocket = UdpSocket { fd : Int, addr : SocketAddr }

# -- Address helpers (sync, pure) --

# ipv4 : String -> Int -> SocketAddr
ipv4 host p = SocketAddr { host = host, port = p }

# ipv6 : String -> Int -> SocketAddr
ipv6 host p = SocketAddr { host = host, port = p }

# addr : SocketAddr -> String
addr sa = sa.host

# port : SocketAddr -> Int
port sa = sa.port

# -- TCP Server (async) --

# tcpListen : String -> Int -> Result TcpListener NetError
# tcpListen host port
extern async yona_Std_Net__tcpListen : String -> Int -> Int

# tcpAccept : TcpListener -> Result TcpStream NetError
# Blocks until a connection arrives
extern async yona_Std_Net__tcpAccept : Int -> Int

# -- TCP Client (async) --

# tcpConnect : String -> Int -> Result TcpStream NetError
# tcpConnect host port
extern async yona_Std_Net__tcpConnect : String -> Int -> Int

# tcpRead : Int -> TcpStream -> Result String NetError
# Read up to n bytes
extern async yona_Std_Net__tcpRead : Int -> Int -> String

# tcpWrite : String -> TcpStream -> Result Unit NetError
extern async yona_Std_Net__tcpWrite : String -> Int -> Unit

# tcpClose : TcpStream -> Result Unit NetError
extern async yona_Std_Net__tcpClose : Int -> Unit

# tcpSetTimeout : Int -> TcpStream -> Result Unit NetError
# Set read/write timeout in milliseconds
extern async yona_Std_Net__tcpSetTimeout : Int -> Int -> Unit

# tcpLocalAddr : TcpStream -> SocketAddr
tcpLocalAddr stream = stream.localAddr

# tcpRemoteAddr : TcpStream -> SocketAddr
tcpRemoteAddr stream = stream.remoteAddr

# -- UDP (async) --

# udpBind : String -> Int -> Result UdpSocket NetError
extern async yona_Std_Net__udpBind : String -> Int -> Int

# udpSendTo : String -> SocketAddr -> UdpSocket -> Result Int NetError
# Returns bytes sent
extern async yona_Std_Net__udpSendTo : String -> String -> Int -> Int -> Int

# udpRecvFrom : Int -> UdpSocket -> Result (String, SocketAddr) NetError
# Read up to n bytes, returns (data, sender_addr)
extern async yona_Std_Net__udpRecvFrom : Int -> Int -> Int

# udpClose : UdpSocket -> Result Unit NetError
extern async yona_Std_Net__udpClose : Int -> Unit

# -- DNS (async) --

# resolve : String -> Result SocketAddr NetError
# Returns first resolved address
extern async yona_Std_Net__resolve : String -> String

# resolveAll : String -> Result Seq NetError
# Returns all resolved addresses
extern async yona_Std_Net__resolveAll : String -> Seq

end
```

---

## Std\Http

```yona
module Std\Http exports
  # Request types
  Request, request,
  Method, GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS,
  withMethod, withHeader, withHeaders, withBody, withTimeout,
  withFollowRedirects, withBasicAuth, withBearerToken,
  # Response types
  Response, status, headers, body, header,
  isOk, isRedirect, isClientError, isServerError,
  # Client
  send, get, post, put, delete, patch,
  # Streaming
  sendStreaming, StreamingResponse, readChunk, closeStream,
  # Server
  Server, serve, ServerConfig, defaultConfig,
  Handler, Route, route, routePrefix,
  respond, respondJson, respondFile, respondStream,
  ResponseBuilder, responseBuilder, withStatus, withResponseHeader, withResponseBody,
  # Error
  HttpError, Timeout, ConnectionFailed, TlsError, InvalidUrl, TooManyRedirects
as

type HttpError
  = Timeout String
  | ConnectionFailed String
  | TlsError String
  | InvalidUrl String
  | TooManyRedirects String
  | HttpIOError { message : String, code : Int }

type Method = GET | POST | PUT | DELETE | PATCH | HEAD | OPTIONS

type Request = Request {
  method : Method,
  url : String,
  hdrs : Seq,
  reqBody : String,
  timeout : Int,
  followRedirects : Bool,
  maxRedirects : Int
}

type Response = Response {
  statusCode : Int,
  respHeaders : Seq,
  respBody : String
}

type StreamingResponse = StreamingResponse { handle : Int, statusCode : Int, respHeaders : Seq }

# -- Request Builder (sync, pure) --

# request : String -> Request
# Creates a GET request to the given URL
request url = Request {
  method = GET,
  url = url,
  hdrs = [],
  reqBody = "",
  timeout = 30000,
  followRedirects = true,
  maxRedirects = 10
}

# withMethod : Method -> Request -> Request
withMethod m req = req { method = m }

# withHeader : String -> String -> Request -> Request
# withHeader name value req
withHeader name value req = req { hdrs = req.hdrs ++ [(name, value)] }

# withHeaders : Seq -> Request -> Request
withHeaders hs req = req { hdrs = req.hdrs ++ hs }

# withBody : String -> Request -> Request
withBody b req = req { reqBody = b }

# withTimeout : Int -> Request -> Request
# Timeout in milliseconds
withTimeout ms req = req { timeout = ms }

# withFollowRedirects : Bool -> Request -> Request
withFollowRedirects follow req = req { followRedirects = follow }

# withBasicAuth : String -> String -> Request -> Request
withBasicAuth user pass req =
  let encoded = Encoding.base64Encode (user ++ ":" ++ pass) in
  withHeader "Authorization" ("Basic " ++ encoded) req

# withBearerToken : String -> Request -> Request
withBearerToken token req =
  withHeader "Authorization" ("Bearer " ++ token) req

# -- Response Query (sync, pure) --

# status : Response -> Int
status resp = resp.statusCode

# headers : Response -> Seq
headers resp = resp.respHeaders

# body : Response -> String
body resp = resp.respBody

# header : String -> Response -> Option String
# Find first header by name (case-insensitive)
header name resp =
  let found = filter (\(n, _) -> toLowerCase n == toLowerCase name) resp.respHeaders in
  case found of
    [] -> None
    [(_, v) | _] -> Some v
  end

# isOk : Response -> Bool
isOk resp = resp.statusCode >= 200 && resp.statusCode < 300

# isRedirect : Response -> Bool
isRedirect resp = resp.statusCode >= 300 && resp.statusCode < 400

# isClientError : Response -> Bool
isClientError resp = resp.statusCode >= 400 && resp.statusCode < 500

# isServerError : Response -> Bool
isServerError resp = resp.statusCode >= 500 && resp.statusCode < 600

# -- HTTP Client (all async) --

# send : Request -> Result Response HttpError
# The primary function -- sends any request
extern async yona_Std_Http__send : Int -> Int

# Convenience wrappers:

# get : String -> Result Response HttpError
get url = send (request url)

# post : String -> String -> Result Response HttpError
post url reqBody = send (request url |> withMethod POST |> withBody reqBody)

# put : String -> String -> Result Response HttpError
put url reqBody = send (request url |> withMethod PUT |> withBody reqBody)

# delete : String -> Result Response HttpError
delete url = send (request url |> withMethod DELETE)

# patch : String -> String -> Result Response HttpError
patch url reqBody = send (request url |> withMethod PATCH |> withBody reqBody)

# -- Streaming (async) --

# sendStreaming : Request -> Result StreamingResponse HttpError
# Returns a streaming handle for large response bodies
extern async yona_Std_Http__sendStreaming : Int -> Int

# readChunk : Int -> StreamingResponse -> Result String HttpError
# Read up to n bytes from streaming response
extern async yona_Std_Http__readChunk : Int -> Int -> String

# closeStream : StreamingResponse -> Result Unit HttpError
extern async yona_Std_Http__closeStream : Int -> Unit

# -- HTTP Server (async) --

type ServerConfig = ServerConfig {
  host : String,
  srvPort : Int,
  maxConnections : Int,
  readTimeout : Int,
  writeTimeout : Int
}

type Server = Server { handle : Int, config : ServerConfig }

type Route = Route { pattern : String, handler : Int }

type ResponseBuilder = ResponseBuilder {
  rbStatus : Int,
  rbHeaders : Seq,
  rbBody : String
}

# defaultConfig : ServerConfig
defaultConfig = ServerConfig {
  host = "0.0.0.0",
  srvPort = 8080,
  maxConnections = 1024,
  readTimeout = 30000,
  writeTimeout = 30000
}

# responseBuilder : ResponseBuilder
responseBuilder = ResponseBuilder { rbStatus = 200, rbHeaders = [], rbBody = "" }

# withStatus : Int -> ResponseBuilder -> ResponseBuilder
withStatus code rb = rb { rbStatus = code }

# withResponseHeader : String -> String -> ResponseBuilder -> ResponseBuilder
withResponseHeader name value rb = rb { rbHeaders = rb.rbHeaders ++ [(name, value)] }

# withResponseBody : String -> ResponseBuilder -> ResponseBuilder
withResponseBody b rb = rb { rbBody = b }

# serve : ServerConfig -> Seq -> Result Server HttpError
# serve config routes -> starts listening
# Each route is (method_string, path_pattern, handler_function)
extern async yona_Std_Http__serve : Int -> Int

# respond : Int -> String -> String -> Response
respond code contentType content =
  Response {
    statusCode = code,
    respHeaders = [("Content-Type", contentType)],
    respBody = content
  }

# respondJson : Int -> String -> Response
respondJson code jsonBody = respond code "application/json" jsonBody

# respondFile : Int -> String -> String -> Response
respondFile code contentType filePath =
  case File.readFile filePath of
    Ok content -> respond code contentType content
    Err _ -> respond 404 "text/plain" "Not Found"
  end

end
```

---

## Std\Json

```yona
module Std\Json exports
  # Core types
  Json, JNull, JBool, JInt, JFloat, JString, JArray, JObject,
  # Parse / Stringify
  parse, stringify, prettyPrint,
  # Construction
  null, bool, int, float, string, array, object,
  # Query
  get, getPath, getInt, getFloat, getString, getBool, getArray, getObject,
  keys, values, entries, hasKey, length,
  # Transform
  mapValues, filterKeys, merge, set, remove,
  # Error
  JsonError, ParseError, TypeError, KeyNotFound
as

type Json
  = JNull
  | JBool Bool
  | JInt Int
  | JFloat Float
  | JString String
  | JArray Seq
  | JObject Seq

type JsonError
  = ParseError { message : String, position : Int }
  | TypeError { expected : String, got : String }
  | KeyNotFound String
  | IndexOutOfBounds Int

# -- Parse / Stringify (sync) --

# parse : String -> Result Json JsonError
extern yona_Std_Json__parse : String -> Int
# Wrapped for Result type

# stringify : Json -> String
extern yona_Std_Json__stringify : Int -> String

# prettyPrint : Int -> Json -> String
# prettyPrint indentSize json
extern yona_Std_Json__prettyPrint : Int -> Int -> String

# -- Construction (sync, pure) --

null = JNull
bool b = JBool b
int n = JInt n
float f = JFloat f
string s = JString s
array items = JArray items
object pairs = JObject pairs

# -- Query (sync, pure) --

# get : String -> Json -> Result Json JsonError
# Get value at key in object
get key json =
  case json of
    JObject pairs ->
      let found = filter (\(k, _) -> k == key) pairs in
      case found of
        [] -> Err (KeyNotFound key)
        [(_, v) | _] -> Ok v
      end
    _ -> Err (TypeError { expected = "JObject", got = "other" })
  end

# getPath : Seq -> Json -> Result Json JsonError
# Navigate nested objects: getPath ["user", "name"] json
getPath path json =
  case path of
    [] -> Ok json
    [key | rest] ->
      case get key json of
        Ok child -> getPath rest child
        Err e -> Err e
      end
  end

# getInt : String -> Json -> Result Int JsonError
getInt key json =
  case get key json of
    Ok (JInt n) -> Ok n
    Ok _ -> Err (TypeError { expected = "JInt", got = "other" })
    Err e -> Err e
  end

# getFloat : String -> Json -> Result Float JsonError
getFloat key json =
  case get key json of
    Ok (JFloat f) -> Ok f
    Ok (JInt n) -> Ok (Std\Types.toFloat n)
    Ok _ -> Err (TypeError { expected = "JFloat", got = "other" })
    Err e -> Err e
  end

# getString : String -> Json -> Result String JsonError
getString key json =
  case get key json of
    Ok (JString s) -> Ok s
    Ok _ -> Err (TypeError { expected = "JString", got = "other" })
    Err e -> Err e
  end

# getBool : String -> Json -> Result Bool JsonError
getBool key json =
  case get key json of
    Ok (JBool b) -> Ok b
    Ok _ -> Err (TypeError { expected = "JBool", got = "other" })
    Err e -> Err e
  end

# getArray : String -> Json -> Result Seq JsonError
getArray key json =
  case get key json of
    Ok (JArray items) -> Ok items
    Ok _ -> Err (TypeError { expected = "JArray", got = "other" })
    Err e -> Err e
  end

# getObject : String -> Json -> Result Seq JsonError
getObject key json =
  case get key json of
    Ok (JObject pairs) -> Ok pairs
    Ok _ -> Err (TypeError { expected = "JObject", got = "other" })
    Err e -> Err e
  end

# keys : Json -> Result Seq JsonError
keys json =
  case json of
    JObject pairs -> Ok (map fst pairs)
    _ -> Err (TypeError { expected = "JObject", got = "other" })
  end

# values : Json -> Result Seq JsonError
values json =
  case json of
    JObject pairs -> Ok (map snd pairs)
    _ -> Err (TypeError { expected = "JObject", got = "other" })
  end

# entries : Json -> Result Seq JsonError
entries json =
  case json of
    JObject pairs -> Ok pairs
    _ -> Err (TypeError { expected = "JObject", got = "other" })
  end

# hasKey : String -> Json -> Bool
hasKey key json =
  case get key json of
    Ok _ -> true
    Err _ -> false
  end

# length : Json -> Result Int JsonError
length json =
  case json of
    JArray items -> Ok (List.length items)
    JObject pairs -> Ok (List.length pairs)
    JString s -> Ok (String.length s)
    _ -> Err (TypeError { expected = "JArray or JObject", got = "other" })
  end

# -- Transform (sync, pure) --

# set : String -> Json -> Json -> Result Json JsonError
# set key value object
set key val json =
  case json of
    JObject pairs ->
      let updated = map (\(k, v) -> if k == key then (k, val) else (k, v)) pairs in
      if any (\(k, _) -> k == key) pairs then Ok (JObject updated)
      else Ok (JObject (pairs ++ [(key, val)]))
    _ -> Err (TypeError { expected = "JObject", got = "other" })
  end

# remove : String -> Json -> Result Json JsonError
remove key json =
  case json of
    JObject pairs -> Ok (JObject (filter (\(k, _) -> k != key) pairs))
    _ -> Err (TypeError { expected = "JObject", got = "other" })
  end

# mapValues : (Json -> Json) -> Json -> Result Json JsonError
mapValues fn json =
  case json of
    JObject pairs -> Ok (JObject (map (\(k, v) -> (k, fn v)) pairs))
    JArray items -> Ok (JArray (map fn items))
    _ -> Err (TypeError { expected = "JObject or JArray", got = "other" })
  end

# filterKeys : (String -> Bool) -> Json -> Result Json JsonError
filterKeys pred json =
  case json of
    JObject pairs -> Ok (JObject (filter (\(k, _) -> pred k) pairs))
    _ -> Err (TypeError { expected = "JObject", got = "other" })
  end

# merge : Json -> Json -> Result Json JsonError
# Merge two objects (right-biased)
merge a b =
  case (a, b) of
    (JObject pa, JObject pb) ->
      let merged = fold (\acc (k, v) -> set k v (JObject acc) |> unwrapOr (JObject acc)) pa pb in
      Ok merged
    _ -> Err (TypeError { expected = "JObject", got = "other" })
  end

end
```

---

## Std\Process

```yona
module Std\Process exports
  # Spawn
  spawn, spawnShell, Process,
  # I/O
  processStdin, processStdout, processStderr,
  writeStdin, readStdout, readStderr, closeStdin,
  # Control
  wait, kill, isAlive, pid,
  # Convenience
  exec, execWithStatus, execCapture,
  # Signals
  Signal, SIGTERM, SIGKILL, SIGINT, SIGHUP,
  sendSignal,
  # Current process
  getArgs, getEnv, setEnv, unsetEnv, getCwd, exit,
  # Error
  ProcessError, SpawnFailed, NotRunning, SignalFailed
as

type ProcessError
  = SpawnFailed String
  | NotRunning Int
  | SignalFailed { signal : Int, pid : Int }
  | ProcessIOError String

type Process = Process {
  handle : Int,
  procPid : Int,
  stdinFd : Int,
  stdoutFd : Int,
  stderrFd : Int
}

type ExitStatus = ExitStatus { code : Int, signaled : Bool }

type Signal = SIGTERM | SIGKILL | SIGINT | SIGHUP

# -- Spawn (async) --

# spawn : String -> Seq -> Result Process ProcessError
# spawn command args
extern async yona_Std_Process__spawn : String -> Int -> Int

# spawnShell : String -> Result Process ProcessError
# Run command through shell
extern async yona_Std_Process__spawnShell : String -> Int

# -- Process I/O (async) --

# writeStdin : String -> Process -> Result Unit ProcessError
extern async yona_Std_Process__writeStdin : String -> Int -> Unit

# readStdout : Int -> Process -> Result String ProcessError
# Read up to n bytes
extern async yona_Std_Process__readStdout : Int -> Int -> String

# readStderr : Int -> Process -> Result String ProcessError
extern async yona_Std_Process__readStderr : Int -> Int -> String

# closeStdin : Process -> Result Unit ProcessError
extern async yona_Std_Process__closeStdin : Int -> Unit

# -- Control (async) --

# wait : Process -> Result ExitStatus ProcessError
# Block until process exits
extern async yona_Std_Process__wait : Int -> Int

# kill : Process -> Result Unit ProcessError
# Send SIGKILL
extern async yona_Std_Process__kill : Int -> Unit

# isAlive : Process -> Bool
extern async yona_Std_Process__isAlive : Int -> Bool

# pid : Process -> Int
pid proc = proc.procPid

# sendSignal : Signal -> Process -> Result Unit ProcessError
extern async yona_Std_Process__sendSignal : Int -> Int -> Unit

# -- Convenience (async) --

# exec : String -> Seq -> Result String ProcessError
# Run command, wait, return stdout
exec cmd args =
  case spawn cmd args of
    Ok proc ->
      case wait proc of
        Ok (ExitStatus { code = 0 }) -> readStdout 1048576 proc
        Ok (ExitStatus { code = c }) -> Err (ProcessIOError ("Exit code: " ++ fromInt c))
        Err e -> Err e
      end
    Err e -> Err e
  end

# execWithStatus : String -> Seq -> Result (Int, String, String) ProcessError
# Returns (exitCode, stdout, stderr)
extern async yona_Std_Process__execWithStatus : String -> Int -> Int

# execCapture : String -> Result String ProcessError
# Run shell command and capture output
execCapture cmd =
  case spawnShell cmd of
    Ok proc ->
      let out = readStdout 1048576 proc in
      let _ = wait proc in
      Ok out
    Err e -> Err e
  end

# -- Current Process (sync unless noted) --

# getArgs : Unit -> Seq
extern yona_Std_Process__getArgs : Unit -> Seq

# getEnv : String -> Option String
extern yona_Std_Process__getEnv : String -> String

# setEnv : String -> String -> Unit
extern yona_Std_Process__setEnv : String -> String -> Unit

# unsetEnv : String -> Unit
extern yona_Std_Process__unsetEnv : String -> Unit

# getCwd : Unit -> String
extern yona_Std_Process__getCwd : Unit -> String

# exit : Int -> Unit
extern yona_Std_Process__exit : Int -> Unit

end
```

---

## Std\Crypto

```yona
module Std\Crypto exports
  # Hashing
  sha256, sha512, sha1, md5,
  sha256Bytes, sha512Bytes,
  # HMAC
  hmacSha256, hmacSha512,
  # Random
  randomBytes, randomInt, randomFloat,
  # UUID
  uuid4, uuidFromString, uuidToString,
  # Constant-time comparison
  secureCompare,
  # Error
  CryptoError, InvalidInput, UnsupportedAlgorithm
as

type CryptoError
  = InvalidInput String
  | UnsupportedAlgorithm String

# -- Hashing (sync) --
# All hash functions take a String and return hex-encoded digest

# sha256 : String -> String
extern yona_Std_Crypto__sha256 : String -> String

# sha512 : String -> String
extern yona_Std_Crypto__sha512 : String -> String

# sha1 : String -> String
extern yona_Std_Crypto__sha1 : String -> String

# md5 : String -> String
extern yona_Std_Crypto__md5 : String -> String

# sha256Bytes : Seq -> String
# Hash raw byte sequence
extern yona_Std_Crypto__sha256Bytes : Seq -> String

# sha512Bytes : Seq -> String
extern yona_Std_Crypto__sha512Bytes : Seq -> String

# -- HMAC (sync) --

# hmacSha256 : String -> String -> String
# hmacSha256 key message -> hex digest
extern yona_Std_Crypto__hmacSha256 : String -> String -> String

# hmacSha512 : String -> String -> String
extern yona_Std_Crypto__hmacSha512 : String -> String -> String

# -- Random (sync -- uses OS entropy) --

# randomBytes : Int -> Seq
# Generate n cryptographically secure random bytes
extern yona_Std_Crypto__randomBytes : Int -> Seq

# randomInt : Int -> Int -> Int
# Random integer in [min, max)
extern yona_Std_Crypto__randomInt : Int -> Int -> Int

# randomFloat : Unit -> Float
# Random float in [0.0, 1.0)
extern yona_Std_Crypto__randomFloat : Unit -> Float

# -- UUID (sync) --

# uuid4 : Unit -> String
# Generate random UUID v4
extern yona_Std_Crypto__uuid4 : Unit -> String

# uuidFromString : String -> Result String CryptoError
uuidFromString s =
  if String.length s == 36 then Ok s
  else Err (InvalidInput "Invalid UUID format")

# uuidToString : String -> String
uuidToString uuid = uuid

# -- Constant-time comparison --

# secureCompare : String -> String -> Bool
# Constant-time string comparison (prevents timing attacks)
extern yona_Std_Crypto__secureCompare : String -> String -> Bool

end
```

---

## Std\Encoding

```yona
module Std\Encoding exports
  # Base64
  base64Encode, base64Decode, base64UrlEncode, base64UrlDecode,
  # Hex
  hexEncode, hexDecode,
  # URL
  urlEncode, urlDecode, urlEncodeComponent, urlDecodeComponent,
  # HTML
  htmlEscape, htmlUnescape,
  # Error
  EncodingError, InvalidBase64, InvalidHex, InvalidUrlEncoding
as

type EncodingError
  = InvalidBase64 String
  | InvalidHex String
  | InvalidUrlEncoding String

# -- Base64 (sync) --

# base64Encode : String -> String
extern yona_Std_Encoding__base64Encode : String -> String

# base64Decode : String -> Result String EncodingError
extern yona_Std_Encoding__base64Decode : String -> String
# Wrapped: returns Result

# base64UrlEncode : String -> String
# URL-safe base64 (uses - and _ instead of + and /)
extern yona_Std_Encoding__base64UrlEncode : String -> String

# base64UrlDecode : String -> Result String EncodingError
extern yona_Std_Encoding__base64UrlDecode : String -> String

# -- Hex (sync) --

# hexEncode : String -> String
# Encode string to hex representation
extern yona_Std_Encoding__hexEncode : String -> String

# hexDecode : String -> Result String EncodingError
extern yona_Std_Encoding__hexDecode : String -> String

# -- URL encoding (sync) --

# urlEncode : String -> String
# Percent-encode a full URL
extern yona_Std_Encoding__urlEncode : String -> String

# urlDecode : String -> Result String EncodingError
extern yona_Std_Encoding__urlDecode : String -> String

# urlEncodeComponent : String -> String
# Encode a single query component (more aggressive than urlEncode)
extern yona_Std_Encoding__urlEncodeComponent : String -> String

# urlDecodeComponent : String -> Result String EncodingError
extern yona_Std_Encoding__urlDecodeComponent : String -> String

# -- HTML escaping (sync) --

# htmlEscape : String -> String
# Escapes <, >, &, ", '
extern yona_Std_Encoding__htmlEscape : String -> String

# htmlUnescape : String -> String
extern yona_Std_Encoding__htmlUnescape : String -> String

end
```

---

## Std\Concurrent

```yona
module Std\Concurrent exports
  # Channels
  Channel, channel, send, receive, tryReceive, closeChannel,
  BufferedChannel, bufferedChannel,
  # Semaphore
  Semaphore, semaphore, acquire, release, tryAcquire,
  withSemaphore,
  # Atomic
  AtomicInt, atomicInt, atomicLoad, atomicStore,
  atomicAdd, atomicSub, atomicCompareAndSwap,
  # STM
  TVar, tvar, readTVar, writeTVar, atomically, retry, orElse,
  # Utilities
  sleep, timeout, race, all, any,
  # Timer
  Timer, setInterval, setTimeout, cancel,
  # Error
  ConcurrentError, ChannelClosed, TimeoutExpired, STMConflict
as

type ConcurrentError
  = ChannelClosed String
  | TimeoutExpired Int
  | STMConflict String

# -- Channels (async, Go-style) --

type Channel = Channel { handle : Int }
type BufferedChannel = BufferedChannel { handle : Int, capacity : Int }

# channel : Unit -> Channel
# Unbuffered (synchronous) channel
extern yona_Std_Concurrent__channel : Unit -> Int

# bufferedChannel : Int -> BufferedChannel
# Buffered channel with given capacity
extern yona_Std_Concurrent__bufferedChannel : Int -> Int

# send : a -> Channel -> Result Unit ConcurrentError
# Blocks until receiver is ready (unbuffered) or buffer has space (buffered)
extern async yona_Std_Concurrent__send : Int -> Int -> Unit

# receive : Channel -> Result a ConcurrentError
# Blocks until a value is available
extern async yona_Std_Concurrent__receive : Int -> Int

# tryReceive : Channel -> Option a
# Non-blocking receive, returns None if nothing available
extern yona_Std_Concurrent__tryReceive : Int -> Int

# closeChannel : Channel -> Unit
extern yona_Std_Concurrent__closeChannel : Int -> Unit

# -- Semaphore (async) --

type Semaphore = Semaphore { handle : Int, permits : Int }

# semaphore : Int -> Semaphore
# Create with n permits
extern yona_Std_Concurrent__semaphore : Int -> Int

# acquire : Semaphore -> Result Unit ConcurrentError
extern async yona_Std_Concurrent__acquire : Int -> Unit

# release : Semaphore -> Unit
extern yona_Std_Concurrent__release : Int -> Unit

# tryAcquire : Semaphore -> Bool
extern yona_Std_Concurrent__tryAcquire : Int -> Bool

# withSemaphore : Semaphore -> (Unit -> a) -> Result a ConcurrentError
# Acquire, run function, release (even on exception)
withSemaphore sem fn =
  let _ = acquire sem in
  try
    let result = fn () in
    let _ = release sem in
    Ok result
  catch e ->
    let _ = release sem in
    raise e
  end

# -- Atomic operations (sync, lock-free) --

type AtomicInt = AtomicInt { handle : Int }

# atomicInt : Int -> AtomicInt
extern yona_Std_Concurrent__atomicInt : Int -> Int

# atomicLoad : AtomicInt -> Int
extern yona_Std_Concurrent__atomicLoad : Int -> Int

# atomicStore : Int -> AtomicInt -> Unit
extern yona_Std_Concurrent__atomicStore : Int -> Int -> Unit

# atomicAdd : Int -> AtomicInt -> Int
# Returns previous value
extern yona_Std_Concurrent__atomicAdd : Int -> Int -> Int

# atomicSub : Int -> AtomicInt -> Int
extern yona_Std_Concurrent__atomicSub : Int -> Int -> Int

# atomicCompareAndSwap : Int -> Int -> AtomicInt -> Bool
# atomicCompareAndSwap expected desired atomic -> succeeded?
extern yona_Std_Concurrent__atomicCAS : Int -> Int -> Int -> Bool

# -- STM (Software Transactional Memory) --

type TVar = TVar { handle : Int }

# tvar : a -> TVar
extern yona_Std_Concurrent__tvar : Int -> Int

# readTVar : TVar -> a
# Only valid inside atomically
extern yona_Std_Concurrent__readTVar : Int -> Int

# writeTVar : a -> TVar -> Unit
extern yona_Std_Concurrent__writeTVar : Int -> Int -> Unit

# atomically : (Unit -> a) -> a
# Execute STM transaction
extern yona_Std_Concurrent__atomically : Int -> Int

# retry : Unit -> a
# Retry current STM transaction (block until a TVar changes)
extern yona_Std_Concurrent__retry : Unit -> Int

# -- Utilities (async) --

# sleep : Int -> Unit
# Sleep for n milliseconds
extern async yona_Std_Concurrent__sleep : Int -> Unit

# timeout : Int -> (Unit -> a) -> Result a ConcurrentError
# Run function with timeout in milliseconds
extern async yona_Std_Concurrent__timeout : Int -> Int -> Int

# race : Seq -> a
# Run multiple thunks concurrently, return first to complete
extern async yona_Std_Concurrent__race : Seq -> Int

# all : Seq -> Seq
# Run multiple thunks concurrently, wait for all results
extern async yona_Std_Concurrent__all : Seq -> Seq

# any : Seq -> a
# Run multiple thunks, return first successful result
extern async yona_Std_Concurrent__any : Seq -> Int

# -- Timer --

type Timer = Timer { handle : Int }

# setInterval : Int -> (Unit -> Unit) -> Timer
# Run function every n milliseconds
extern yona_Std_Concurrent__setInterval : Int -> Int -> Int

# setTimeout : Int -> (Unit -> Unit) -> Timer
# Run function after n milliseconds
extern yona_Std_Concurrent__setTimeout : Int -> Int -> Int

# cancel : Timer -> Unit
extern yona_Std_Concurrent__cancel : Int -> Unit

end
```

---

## Std\Collection

```yona
module Std\Collection exports
  # Seq extras (beyond Std\List)
  zip, zipWith, unzip, enumerate, intersperse,
  chunk, window, unique, sortBy, groupBy,
  partition, span, scanl, scanr,
  findIndex, elemIndex, intercalate,
  # Dict
  dictGet, dictPut, dictRemove, dictMerge, dictKeys, dictValues,
  dictEntries, dictFromEntries, dictMap, dictFilter,
  dictFold, dictContainsKey, dictSize, dictIsEmpty,
  dictUpdate, dictMapKeys, dictMapValues,
  # Set
  setAdd, setRemove, setUnion, setIntersection, setDifference,
  setSymDifference, setIsSubset, setIsSuperset,
  setMap, setFilter, setFold, setSize, setIsEmpty,
  setToList, setFromList, setContains,
  # SortedSet
  SortedSet, sortedSet, sortedSetAdd, sortedSetRemove,
  sortedSetMin, sortedSetMax, sortedSetRange,
  # Priority Queue
  PriorityQueue, PQ, PQEmpty,
  pqNew, pqPush, pqPop, pqPeek, pqSize, pqIsEmpty, pqFromList,
  # Deque
  Deque, dequeNew, dequePushFront, dequePushBack,
  dequePopFront, dequePopBack, dequePeekFront, dequePeekBack,
  dequeSize, dequeIsEmpty, dequeToList
as

type CollectionError = EmptyCollection String | KeyNotFound String | IndexOutOfBounds Int

# -- Seq extras (sync, pure) --

# zip : Seq -> Seq -> Seq
# zip [1,2,3] ["a","b","c"] -> [(1,"a"), (2,"b"), (3,"c")]
zip as bs =
  case (as, bs) of
    ([], _) -> []
    (_, []) -> []
    ([a|at], [b|bt]) -> (a, b) :: zip at bt
  end

# zipWith : (a -> b -> c) -> Seq -> Seq -> Seq
zipWith fn as bs =
  case (as, bs) of
    ([], _) -> []
    (_, []) -> []
    ([a|at], [b|bt]) -> fn a b :: zipWith fn at bt
  end

# unzip : Seq -> (Seq, Seq)
unzip pairs =
  case pairs of
    [] -> ([], [])
    [(a, b) | rest] ->
      let (as, bs) = unzip rest in
      (a :: as, b :: bs)
  end

# enumerate : Seq -> Seq
# enumerate ["a","b","c"] -> [(0,"a"), (1,"b"), (2,"c")]
enumerate seq =
  let go idx remaining =
    case remaining of
      [] -> []
      [h | t] -> (idx, h) :: go (idx + 1) t
    end
  in go 0 seq

# intersperse : a -> Seq -> Seq
# intersperse 0 [1,2,3] -> [1,0,2,0,3]
intersperse sep seq =
  case seq of
    [] -> []
    [x] -> [x]
    [x | rest] -> x :: sep :: intersperse sep rest
  end

# chunk : Int -> Seq -> Seq
# chunk 2 [1,2,3,4,5] -> [[1,2],[3,4],[5]]
chunk n seq =
  case seq of
    [] -> []
    _ -> List.take n seq :: chunk n (List.drop n seq)
  end

# window : Int -> Seq -> Seq
# Sliding window: window 3 [1,2,3,4,5] -> [[1,2,3],[2,3,4],[3,4,5]]
window n seq =
  if List.length seq < n then []
  else List.take n seq :: window n (List.tail seq)

# unique : Seq -> Seq
# Remove duplicates (preserves first occurrence order)
unique seq =
  case seq of
    [] -> []
    [h | t] -> if List.contains h t then unique t else h :: unique t
  end

# sortBy : (a -> a -> Int) -> Seq -> Seq
# Comparison function returns negative, 0, or positive
extern yona_Std_Collection__sortBy : Int -> Seq -> Seq

# groupBy : (a -> k) -> Seq -> Seq
# Returns Seq of (key, Seq) pairs
extern yona_Std_Collection__groupBy : Int -> Seq -> Seq

# partition : (a -> Bool) -> Seq -> (Seq, Seq)
# Split into (matching, not-matching)
partition pred seq =
  case seq of
    [] -> ([], [])
    [h | t] ->
      let (yes, no) = partition pred t in
      if pred h then (h :: yes, no) else (yes, h :: no)
  end

# span : (a -> Bool) -> Seq -> (Seq, Seq)
# Take longest prefix satisfying pred
span pred seq =
  case seq of
    [] -> ([], [])
    [h | t] ->
      if pred h then
        let (taken, rest) = span pred t in
        (h :: taken, rest)
      else
        ([], seq)
  end

# scanl : (b -> a -> b) -> b -> Seq -> Seq
# Like foldl but returns all intermediate values
scanl fn acc seq =
  acc :: (case seq of
    [] -> []
    [h | t] -> scanl fn (fn acc h) t
  end)

# scanr : (a -> b -> b) -> b -> Seq -> Seq
extern yona_Std_Collection__scanr : Int -> Int -> Seq -> Seq

# findIndex : (a -> Bool) -> Seq -> Option Int
findIndex pred seq =
  let go idx remaining =
    case remaining of
      [] -> None
      [h | t] -> if pred h then Some idx else go (idx + 1) t
    end
  in go 0 seq

# elemIndex : a -> Seq -> Option Int
elemIndex elem seq = findIndex (\x -> x == elem) seq

# intercalate : Seq -> Seq -> Seq
# intercalate [0,0] [[1,2],[3,4],[5,6]] -> [1,2,0,0,3,4,0,0,5,6]
intercalate sep seqs = List.flatten (intersperse sep seqs)

# -- Dict operations (sync, C shims) --

# dictGet : a -> Dict -> Option b
extern yona_Std_Collection__dictGet : Int -> Int -> Int

# dictPut : a -> b -> Dict -> Dict
extern yona_Std_Collection__dictPut : Int -> Int -> Int -> Int

# dictRemove : a -> Dict -> Dict
extern yona_Std_Collection__dictRemove : Int -> Int -> Int

# dictMerge : Dict -> Dict -> Dict
# Right-biased merge
extern yona_Std_Collection__dictMerge : Int -> Int -> Int

# dictKeys : Dict -> Seq
extern yona_Std_Collection__dictKeys : Int -> Seq

# dictValues : Dict -> Seq
extern yona_Std_Collection__dictValues : Int -> Seq

# dictEntries : Dict -> Seq
# Returns Seq of (key, value) tuples
extern yona_Std_Collection__dictEntries : Int -> Seq

# dictFromEntries : Seq -> Dict
extern yona_Std_Collection__dictFromEntries : Seq -> Int

# dictMap : (a -> b -> c) -> Dict -> Dict
extern yona_Std_Collection__dictMap : Int -> Int -> Int

# dictFilter : (a -> b -> Bool) -> Dict -> Dict
extern yona_Std_Collection__dictFilter : Int -> Int -> Int

# dictFold : (acc -> a -> b -> acc) -> acc -> Dict -> acc
extern yona_Std_Collection__dictFold : Int -> Int -> Int -> Int

# dictContainsKey : a -> Dict -> Bool
extern yona_Std_Collection__dictContainsKey : Int -> Int -> Bool

# dictSize : Dict -> Int
extern yona_Std_Collection__dictSize : Int -> Int

# dictIsEmpty : Dict -> Bool
dictIsEmpty d = dictSize d == 0

# dictUpdate : a -> (Option b -> Option b) -> Dict -> Dict
extern yona_Std_Collection__dictUpdate : Int -> Int -> Int -> Int

# dictMapKeys : (a -> c) -> Dict -> Dict
extern yona_Std_Collection__dictMapKeys : Int -> Int -> Int

# dictMapValues : (b -> c) -> Dict -> Dict
extern yona_Std_Collection__dictMapValues : Int -> Int -> Int

# -- Set operations (sync, C shims) --

# setAdd : a -> Set -> Set
extern yona_Std_Collection__setAdd : Int -> Int -> Int

# setRemove : a -> Set -> Set
extern yona_Std_Collection__setRemove : Int -> Int -> Int

# setUnion : Set -> Set -> Set
extern yona_Std_Collection__setUnion : Int -> Int -> Int

# setIntersection : Set -> Set -> Set
extern yona_Std_Collection__setIntersection : Int -> Int -> Int

# setDifference : Set -> Set -> Set
extern yona_Std_Collection__setDifference : Int -> Int -> Int

# setSymDifference : Set -> Set -> Set
setSymDifference a b = setUnion (setDifference a b) (setDifference b a)

# setIsSubset : Set -> Set -> Bool
extern yona_Std_Collection__setIsSubset : Int -> Int -> Bool

# setIsSuperset : Set -> Set -> Bool
setIsSuperset a b = setIsSubset b a

# setMap : (a -> b) -> Set -> Set
extern yona_Std_Collection__setMap : Int -> Int -> Int

# setFilter : (a -> Bool) -> Set -> Set
extern yona_Std_Collection__setFilter : Int -> Int -> Int

# setFold : (acc -> a -> acc) -> acc -> Set -> acc
extern yona_Std_Collection__setFold : Int -> Int -> Int -> Int

# setSize : Set -> Int
extern yona_Std_Collection__setSize : Int -> Int

# setIsEmpty : Set -> Bool
setIsEmpty s = setSize s == 0

# setToList : Set -> Seq
extern yona_Std_Collection__setToList : Int -> Seq

# setFromList : Seq -> Set
extern yona_Std_Collection__setFromList : Seq -> Int

# setContains : a -> Set -> Bool
extern yona_Std_Collection__setContains : Int -> Int -> Bool

# -- Priority Queue (pure Yona, heap-based via ADT) --

type PriorityQueue a = PQ Int a (PriorityQueue a) (PriorityQueue a) | PQEmpty

pqNew = PQEmpty

pqIsEmpty pq = case pq of PQEmpty -> true; _ -> false end

pqSize pq = case pq of PQEmpty -> 0; PQ _ _ l r -> 1 + pqSize l + pqSize r end

# pqPush : Int -> a -> PriorityQueue a -> PriorityQueue a
# Priority is the Int (lower = higher priority, min-heap)
pqPush priority val pq =
  case pq of
    PQEmpty -> PQ priority val PQEmpty PQEmpty
    PQ p v l r ->
      if priority <= p then PQ priority val (pqPush p v r) l
      else PQ p v (pqPush priority val r) l
  end

# pqPeek : PriorityQueue a -> Option (Int, a)
pqPeek pq =
  case pq of
    PQEmpty -> None
    PQ p v _ _ -> Some (p, v)
  end

# pqPop : PriorityQueue a -> Option ((Int, a), PriorityQueue a)
# Returns Some ((priority, value), remainingQueue)
pqPop pq =
  case pq of
    PQEmpty -> None
    PQ p v l r -> Some ((p, v), pqMerge l r)
  end

# Internal merge helper
pqMerge a b =
  case (a, b) of
    (PQEmpty, q) -> q
    (q, PQEmpty) -> q
    (PQ pa va la ra, PQ pb vb lb rb) ->
      if pa <= pb then PQ pa va (pqMerge ra b) la
      else PQ pb vb (pqMerge a rb) lb
  end

# pqFromList : Seq -> PriorityQueue a
# Takes Seq of (priority, value) pairs
pqFromList pairs =
  List.fold (\acc (p, v) -> pqPush p v acc) PQEmpty pairs

# -- Deque (pure Yona, two-list implementation) --

type Deque = Deque { front : Seq, back : Seq }

dequeNew = Deque { front = [], back = [] }

dequePushFront x dq = dq { front = x :: dq.front }
dequePushBack x dq = dq { back = x :: dq.back }

dequePopFront dq =
  case dq.front of
    [h | t] -> Some (h, dq { front = t })
    [] ->
      case List.reverse dq.back of
        [] -> None
        [h | t] -> Some (h, Deque { front = t, back = [] })
      end
  end

dequePopBack dq =
  case dq.back of
    [h | t] -> Some (h, dq { back = t })
    [] ->
      case List.reverse dq.front of
        [] -> None
        [h | t] -> Some (h, Deque { front = [], back = t })
      end
  end

dequePeekFront dq =
  case dq.front of
    [h | _] -> Some h
    [] -> case List.reverse dq.back of [h | _] -> Some h; [] -> None end
  end

dequePeekBack dq =
  case dq.back of
    [h | _] -> Some h
    [] -> case List.reverse dq.front of [h | _] -> Some h; [] -> None end
  end

dequeSize dq = List.length dq.front + List.length dq.back

dequeIsEmpty dq = dequeSize dq == 0

dequeToList dq = dq.front ++ List.reverse dq.back

end
```

---

## Std\Log

```yona
module Std\Log exports
  # Levels
  Level, Debug, Info, Warn, Error, Fatal,
  # Logger
  Logger, logger, withLevel, withOutput, withFormatter,
  # Log functions
  debug, info, warn, error, fatal,
  # Output
  LogOutput, StdoutOutput, StderrOutput, FileOutput,
  # Formatter
  LogFormatter, TextFormatter, JsonFormatter, CustomFormatter,
  # Entry
  LogEntry, withField, withFields,
  # Global
  setDefaultLogger, getDefaultLogger
as

type Level = Debug | Info | Warn | Error | Fatal

type LogOutput
  = StdoutOutput
  | StderrOutput
  | FileOutput String

type LogFormatter
  = TextFormatter
  | JsonFormatter
  | CustomFormatter (LogEntry -> String)

type LogEntry = LogEntry {
  level : Level,
  message : String,
  timestamp : Int,
  fields : Seq
}

type Logger = Logger {
  minLevel : Level,
  output : LogOutput,
  formatter : LogFormatter,
  defaultFields : Seq
}

# -- Logger construction (sync, pure) --

# logger : Logger
# Default logger: Info level, stderr, text format
logger = Logger {
  minLevel = Info,
  output = StderrOutput,
  formatter = TextFormatter,
  defaultFields = []
}

# withLevel : Level -> Logger -> Logger
withLevel lvl log = log { minLevel = lvl }

# withOutput : LogOutput -> Logger -> Logger
withOutput out log = log { output = out }

# withFormatter : LogFormatter -> Logger -> Logger
withFormatter fmt log = log { formatter = fmt }

# -- Structured fields --

# withField : String -> String -> LogEntry -> LogEntry
withField key value entry = entry { fields = entry.fields ++ [(key, value)] }

# withFields : Seq -> LogEntry -> LogEntry
withFields kvs entry = entry { fields = entry.fields ++ kvs }

# -- Level comparison helper --
levelOrd lvl =
  case lvl of
    Debug -> 0
    Info -> 1
    Warn -> 2
    Error -> 3
    Fatal -> 4
  end

shouldLog loggerLevel msgLevel = levelOrd msgLevel >= levelOrd loggerLevel

# -- Log functions (async, side-effecting) --

# Internal: log at a given level
logAt lvl msg fields lgr =
  if shouldLog lgr.minLevel lvl then
    let entry = LogEntry {
      level = lvl,
      message = msg,
      timestamp = Time.nowMillis (),
      fields = lgr.defaultFields ++ fields
    } in
    let formatted = formatEntry lgr.formatter entry in
    writeOutput lgr.output formatted
  else ()

# debug : String -> Logger -> Unit
debug msg lgr = logAt Debug msg [] lgr

# info : String -> Logger -> Unit
info msg lgr = logAt Info msg [] lgr

# warn : String -> Logger -> Unit
warn msg lgr = logAt Warn msg [] lgr

# error : String -> Logger -> Unit
error msg lgr = logAt Error msg [] lgr

# fatal : String -> Logger -> Unit
fatal msg lgr = logAt Fatal msg [] lgr

# -- Formatting (sync, internal) --
formatEntry fmt entry =
  case fmt of
    TextFormatter ->
      let lvlStr = case entry.level of
        Debug -> "DEBUG"
        Info -> "INFO"
        Warn -> "WARN"
        Error -> "ERROR"
        Fatal -> "FATAL"
      end in
      let fieldsStr = String.join " " (List.map (\(k, v) -> k ++ "=" ++ v) entry.fields) in
      "[{lvlStr}] {entry.message} {fieldsStr}\n"
    JsonFormatter ->
      let fieldsObj = List.map (\(k, v) -> "\"" ++ k ++ "\":\"" ++ v ++ "\"") entry.fields in
      "{\"level\":\"{lvlStr}\",\"message\":\"{entry.message}\",{String.join "," fieldsObj}}\n"
    CustomFormatter fn -> fn entry
  end

# -- Output (async, internal) --
writeOutput out text =
  case out of
    StdoutOutput -> IO.print text
    StderrOutput -> IO.eprint text
    FileOutput path -> File.appendFile path text
  end

# -- Global logger --
# The global logger is stored as a mutable reference (C shim)
extern yona_Std_Log__setDefault : Int -> Unit
extern yona_Std_Log__getDefault : Unit -> Int

end
```

---

## Usage Examples

These examples demonstrate how the modules compose together for real-world tasks.

### Example 1: HTTP API Client

```yona
import send, request, withMethod, POST, withHeader, withBody, body, status from Std\Http in
import parse, getString, getInt from Std\Json in

let apiCall =
  request "https://api.example.com/users"
  |> withMethod POST
  |> withHeader "Content-Type" "application/json"
  |> withBody "{\"name\": \"Alice\", \"age\": 30}"
  |> send
in
case apiCall of
  Ok resp ->
    if status resp >= 200 && status resp < 300 then
      case parse (body resp) of
        Ok json ->
          let id = getInt "id" json |> unwrapOr 0 in
          println "Created user with id: {(fromInt id)}"
        Err e -> eprintln "Failed to parse response"
      end
    else
      eprintln "HTTP error: {(fromInt (status resp))}"
  Err (Timeout _) -> eprintln "Request timed out"
  Err (ConnectionFailed msg) -> eprintln "Connection failed: {msg}"
  Err _ -> eprintln "Unknown error"
end
```

### Example 2: File Processing Pipeline

```yona
import readFile, writeFile, listDir, joinPath, extension from Std\File in
import split, join, trim, contains from Std\String in
import map, filter, fold from Std\List in

let processDir dir =
  case listDir dir of
    Ok files ->
      let csvFiles = filter (\f -> extension f == ".csv") files in
      let results = map (\f ->
        let path = joinPath dir f in
        case readFile path of
          Ok content ->
            let lines = split "\n" content in
            let filtered = filter (\line -> contains "ERROR" line) lines in
            (f, filtered)
          Err e -> (f, [])
        end
      ) csvFiles in
      let report = map (\(name, errors) ->
        "{name}: {(fromInt (length errors))} errors"
      ) results in
      writeFile "report.txt" (join "\n" report)
    Err e -> eprintln "Failed to list directory"
  end
in processDir "/var/log/app"
```

### Example 3: Concurrent Web Scraper

```yona
import get, body from Std\Http in
import all, timeout from Std\Concurrent in
import parse, getArray, getString from Std\Json in
import map from Std\List in

let urls = [
  "https://api.example.com/page/1",
  "https://api.example.com/page/2",
  "https://api.example.com/page/3"
] in

# All HTTP gets run concurrently via transparent async
let fetchers = map (\url -> \-> get url) urls in
let responses = all fetchers in
let results = map (\resp ->
  case resp of
    Ok r -> body r
    Err _ -> ""
  end
) responses in
join "\n" results
```

### Example 4: Structured Logging with JSON

```yona
import logger, withLevel, Debug, withFormatter, JsonFormatter, info, withField from Std\Log in
import sha256 from Std\Crypto in
import uuid4 from Std\Crypto in

let log = logger |> withLevel Debug |> withFormatter JsonFormatter in
let requestId = uuid4 () in

info "Request received" log
  |> withField "request_id" requestId
  |> withField "method" "POST"
  |> withField "path" "/api/users"
```

### Example 5: TCP Echo Server

```yona
import tcpListen, tcpAccept, tcpRead, tcpWrite, tcpClose from Std\Net in
import info, logger from Std\Log in

let log = logger in

let handleClient stream =
  case tcpRead 4096 stream of
    Ok data ->
      if String.isEmpty data then tcpClose stream
      else
        let _ = tcpWrite data stream in
        handleClient stream
    Err _ -> tcpClose stream
  end
in

case tcpListen "0.0.0.0" 8080 of
  Ok listener ->
    let _ = info "Listening on :8080" log in
    let acceptLoop () =
      case tcpAccept listener of
        Ok stream ->
          let _ = handleClient stream in
          acceptLoop ()
        Err e -> info "Accept error" log
      end
    in acceptLoop ()
  Err e -> eprintln "Failed to listen"
end
```

---

## Implementation Strategy

### Phase 1: Core Types and Pure Functions
Files that can be implemented entirely in Yona without C shims:
1. Enhanced `Std\Result` and `Std\Option` (pure Yona)
2. `Std\Collection` -- pure Yona portions (zip, enumerate, partition, PriorityQueue, Deque)
3. `Std\Log` -- pure Yona with calls to existing IO

### Phase 2: String and Encoding (C shims, sync)
4. `Std\String` -- C shims using standard C library (strlen, strstr, strtok, etc.)
5. `Std\Encoding` -- C shims for base64 (custom), URL encoding (custom), HTML escaping

### Phase 3: I/O Foundation (C shims, async)
6. `Std\IO` -- C shims wrapping POSIX read/write/fdopen
7. `Std\File` -- C shims wrapping POSIX open/read/write/stat/opendir
8. `Std\Process` -- C shims wrapping fork/exec/waitpid/pipe

### Phase 4: Networking (C shims, async)
9. `Std\Net` -- C shims wrapping POSIX socket/bind/listen/accept/connect
10. `Std\Http` -- C shims wrapping libcurl (client) and custom HTTP parser (server)

### Phase 5: Data Formats and Security
11. `Std\Json` -- C shims wrapping cJSON or a custom parser
12. `Std\Crypto` -- C shims wrapping OpenSSL or libsodium

### Phase 6: Concurrency Primitives
13. `Std\Concurrent` -- C shims wrapping pthreads (channels, semaphores, atomics)

### Critical Implementation Notes

1. **C shim naming convention**: Functions follow `yona_Std_ModuleName__functionName` as observed in `compiled_runtime.c`.

2. **Async functions**: The existing thread pool in `compiled_runtime.c` uses `yona_rt_async_call_thunk` for multi-arg async. All async stdlib functions will use this same mechanism.

3. **ADT construction/destruction in C**: The runtime already has `yona_rt_adt_alloc`, `yona_rt_adt_get_tag`, `yona_rt_adt_get_field`, `yona_rt_adt_set_field`. C shims that return ADT values (like `Result`) will need to construct these at the C level, or alternatively return raw values and let the Yona wrapper construct the ADT.

4. **Error handling strategy**: For C shims, the recommended approach is to have the C function return a sentinel value (e.g., NULL pointer for errors) and have the Yona wrapper function construct the `Result` ADT. This avoids C code needing to know ADT layout.

5. **Memory management**: Currently all heap allocations leak (per `todo-list.md`). The stdlib design does not assume GC but should be GC-friendly when one is added -- no hidden circular references, clear ownership.

6. **Collection C shims**: Dict and Set operations require C shims because the runtime layout (`[count, key, val, key, val, ...]` for Dict, `[count, elem, ...]` for Set) needs direct manipulation that is not expressible in pure Yona.

---

### Critical Files for Implementation
- `/home/adam/source/repos/yonac-llvm/src/compiled_runtime.c` -- All C shims for stdlib functions must be added here, following the existing naming convention (`yona_Std_Module__function`)
- `/home/adam/source/repos/yonac-llvm/lib/Std/Result.yona` -- Foundation type used by every other module; needs `flatMap`, `collect`, `sequence` additions
- `/home/adam/source/repos/yonac-llvm/lib/Std/Option.yona` -- Foundation type; needs `flatMap`, `filter`, `toResult` additions
- `/home/adam/source/repos/yonac-llvm/src/Codegen.cpp` -- The codegen must handle module-level `extern async` declarations and ensure `.yonai` interface files carry the async/sync distinction and return type metadata for cross-module calls
- `/home/adam/source/repos/yonac-llvm/docs/language-syntax.md` -- Reference for all syntax forms (ADT named fields, `case`/`end`, `do`/`end`, `try`/`catch`/`end`, pipe `|>`, string interpolation `{expr}`, imports) that the stdlib must conform to
