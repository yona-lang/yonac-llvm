# Std.Channel

Bounded MPMC channels for inter-task communication. Send blocks when the
buffer is full, recv blocks when empty. Designed for use between concurrent
tasks (spawned via `Std\Task.spawn`) — single-task usage will deadlock and
is detected at runtime.

## Functions

### channel

```
channel : Int -> Channel a
```

Create a bounded channel with the given buffer capacity.

```yona
import channel from Std\Channel in
let ch = channel 16 in ...
```

### send

```
send : Channel a -> a -> ()
```

Send a value to the channel. **Blocks** the calling task's worker thread
if the buffer is full. Raises `:ChannelClosed` if the channel was closed.

### recv

```
recv : Channel a -> Option a
```

Receive a value from the channel. **Blocks** the calling task's worker
thread if the buffer is empty. Returns:

- `Some v` — value received
- `None` — channel was closed and drained (end of stream)
- raises `:Cancelled` — task group was cancelled
- raises `:Deadlock` — runtime detected no progress is possible

The `None` return is the natural end-of-stream signal — use it to terminate
consumer loops:

```yona
let loop acc = case recv ch of
    Some v -> loop (acc + v)
    None   -> acc
end in loop 0
```

### tryRecv

```
tryRecv : Channel a -> Option a
```

Non-blocking receive. Returns immediately:

- `Some v` if a value is available
- `None` if the channel is empty (open or closed)

### close

```
close : Channel a -> ()
```

Mark the channel as closed. Wakes all blocked sends and recvs:

- Pending sends raise `:ChannelClosed`
- Pending recvs return `Some v` if data is buffered, then `None` after drained

### isClosed

```
isClosed : Channel a -> Bool
```

Returns `true` if the channel has been closed.

### length

```
length : Channel a -> Int
```

Current number of buffered elements.

### capacity

```
capacity : Channel a -> Int
```

Maximum buffer size (set at creation).

## Usage Patterns

### Producer-consumer

```yona
import channel, send, recv, close from Std\Channel in
import spawn from Std\Task in

let ch = channel 16 in
let _ = spawn (\() ->
    let _ = send ch 1 in
    let _ = send ch 2 in
    let _ = send ch 3 in
    close ch) in
let loop acc = case recv ch of
    Some v -> loop (acc + v)
    None   -> acc
end in
loop 0   -- 6
```

### Worker pool (fan-out)

```yona
let work = channel 16 in
let _ = spawn (\() ->
    let send_all n = if n > 100 then close work
                     else let _ = send work n in send_all (n+1)
    in send_all 1) in
-- Multiple workers reading from the same channel
let worker = (\() -> consume work) in
let w1 = spawn worker in
let w2 = spawn worker in
let w3 = spawn worker in
(w1, w2, w3)
```

### Why "between tasks"?

Channels are designed for **inter-task** communication. Using them within
a single task (e.g., calling `recv` without ever sending from another task)
will block forever. The runtime detects this after ~5 seconds and raises
`:Deadlock`.

The canonical pattern is to spawn the producer with `spawn` from `Std\Task`,
then read in the main task or another spawned task.

## Implementation Notes

- Bounded ring buffer with `pthread_mutex_t` + two `pthread_cond_t`
- send/recv block the worker thread; other tasks in the group continue
- Cancellation via task group integration (cancel wakes all waiters)
- Deadlock detection via timed waits + heuristic check
- RC-managed: channel is freed when last reference drops
- Element type is phantom (everything is i64 at runtime); type checker
  enforces type safety
