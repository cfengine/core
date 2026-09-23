# cf-reactor

# High level design

### Tracking

cf-reactor periodically reads the policy to set up the in-memory datastructures needed to react to events. It evaluates reactor bundles and parses events promises to create the corresponding `watchers`. These watchers stay active until the next policy read, when they are rebuilt.

Events promises are parsed into watcher instances, each holding: the event `key` from the events promise, an event `type` (e.g. file deletion), a `state` with whatever state the check needs (e.g. the name of the file being watched), and a `check function` that detects the event by comparing the current state to the state recorded at the last check (e.g. whether the file still exists). It returns `true` only on the specific transition being watched for, so it fires once per change and does not stay `true` afterwards.

### Events

The daemon runs a loop that calls `select(2)` on its file descriptors until one of them signals activity. When a check function fires, it pushes the event key onto a thread-safe `event queue` and writes to the reactor's file descriptor, waking up the `select(2)` loop. This means that for now, an event is just a string.

On wakeup, the daemon drains the event queue and dispatches each event to its bundle using a `hashmap`, built from the policy, that maps event keys to bundles.

### Polling

To remain cross-platform, cf-reactor detects changes with a simple polling thread: it iterates through all watcher instances and calls their check functions to see whether a change happened, then sleeps.

On Linux, we could use inotify instead in the future, and let the kernel poll for us. Its architecture is similar to ours: a single file descriptor is written to on an event, and events are pushed to a queue that must be read. The difference is that inotify maps events to actions using "watch descriptors" (an int) rather than keys, so we would need a translation layer between watch descriptors and event keys.

### Running bundles

On dispatch, the bundle corresponding to an event is run, however it doesn't do a full agent run.

## Implementation details

### Tracking spec & Events

In order to track all the events promises, we use two datastructures: a global list of `"Watcher"`, which is a struct associated with an event type and the promise name (also called `key`) and a global hashmap mapping this `key` to a `bundle` which is parsed from the policy.

cf-reactor reads the policy periodically, and when it does, rebuilds the list of watchers and the hashmap using the single function `WatcherRegister(key, event_type, state, bundle, interval)`. Each events promise is associated with an event type, which is defined in `when` bodies:

```cf3
body when file_deleted(filename)
{
    file_deleted => "$(filename)";
}
```

Every event type must have defined:
- A check function (called `check_callback` in `Watcher`): This is a function defined specifically for the event that checks if the conditions holds. For example, in case of file deletion, we check if the file doesn't exist anymore compare to the last time we checked. If yes, then it returns `true`.
- A `state`: This is a struct whose interpretation depends on the event type (thus being declared as `void *`). We typically need some state that we compare between each event-check. In the case of file deletion, we need to know the name of the file we are watching, and whether the file existed last time we checked.
- A state destroying function (called `destroy_state`): This is simply a function to free the state associated with the event type.

Also, we need a function that will create the state. That's what `FileWatcherPayloadNew()` does.

So each event type we add in the future just need to have these four things defined, and we need to create the `Watcher` object with the right functions inside `WatcherRegister()` and also call the right `"...PayloadNew()"` function.


### Polling & Running bundles

`ReactorContextInitialize()` sets up all the necessary data structures for polling, and then starts `WatcherThreadMain`, which polls for events as follows:

- It iterates through each watcher in the global list of watchers.
- If the elapsed time exceeds the watcher's `interval`, it runs `check_callback` to determine whether an event has been triggered.
- If an event was triggered, it pushes the watcher's `key` (the promise name) onto a thread-safe queue, then signals the file descriptor via `WakeupChannelNotify`, which `select(2)` will pick up on its next iteration. It then goes back to sleep.

In parallel, `EventWatcherHandleEvents`, called from within `ReactorContextHandleEvents`, reads from the file descriptor with `WakeupChannelReadFd()` once notified that an event has occurred, and pops the thread-safe queue until it's empty. Each key popped from the queue is looked up in the global hashmap to retrieve the corresponding bundle, which `cf-reactor` then runs (in another thread or subprocess)
