#pragma once

// WIP - goal of this is to act as the main api header.

/*

Terminology:

Channels
All logs/data are part of a channel.  Channels can be
enabled, enabled-no-output, or completely disabled
when completely disabled, the log portion of the code
is compiled out with constexprs


Function Details
Outputs basic information about this function, pseudo process id,
pseudo thread it, (todo timestamp), channel id....
    

"Using <channel name> channel"
This means that this macro's execution will be based on the 
channel setting, if it's enabled/disabled/etc.
*/

// Easier "Defaults": logs using default channel.

// ***** TEST to see if TLS is actually costly or not since 
// this needs to create a block logger object ANYWAYS
// Uses DEFAULT channel
// Creates a temporary scope and prints the function details to it
#define CAP_MARK()

// Creates a function scope in the current context
// Uses the DEFAULT channel - equivalent to 
// to CAP_SCOPE_CHANNEL(DEFAULT)
#define CAP_SCOPE()

// If an enclosing function scope exists *and* is 
// enabled, will send this log line to the output.
#define CAP_LOG(...)


// Explicit Channel variants

//
#define CAP_CHANNEL_MARK()

// Creates a function scope in the current context
// Uses the channel provided.
#define CAP_CHANNEL_SCOPE(channel)


// Same as CAP_LOG, but if there's no scope 
// (or the enclosing scope's channel is disabled) 
// will create an "anonymous" scope using the provided
// channel, log the message, and then immediately close
// the anonymous scope.
// TODO: mechanism is weird.  Think about it more. 
// - if you disable context's channel this will print
// - if context's channel is enabled, but you disable 
// the specified channel, it will also still print
// - if both context's channel and this channel are
// disabled will it not print
// PROPOSAL:
// For consistency, it might be better always have this 
// channel print using the user channel, but only create
// the scope part if it doesn't exist.
// since log messages and scope blocks have channels.
// Advantage: the log line is effectively turned on/off
// just by one channel.  Easier to name and grok.
// #define CAP_LOG_USE_CHAN_IF_SCOPELESS(channel, ...)
#define CAP_CHANNEL_LOG(channel, ...)

// Behaves similar to CAP_CHANNEL_LOG
// but it uses an "ERROR" channel
#define CAP_ERROR(...)

