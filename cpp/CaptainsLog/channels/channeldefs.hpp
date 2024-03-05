// Do not at pragma or header guard.

/**
 * Channels are defined using the X-Macros technique in a tree configuration.
 * Example of X-Macro trees can be found here: [danilafe.com/blog/chapel_x_macros/](https://danilafe.com/blog/chapel_x_macros/)
 *
 * CAPTAINS_LOG_CHANNEL(name, verbosityLevel, enabled):
 * param name: The name of the channel. Must be unique.
 * param verbosityLevel: larger the number, the more verbose it is.
 * param enabled: true or false if this channel is enabled.
 */

CAPTAINS_LOG_CHANNEL(ALL, 0, true)
  CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN()
    CAPTAINS_LOG_CHANNEL(RENDER, 0, true)
      CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN()
        CAPTAINS_LOG_CHANNEL(RENDER_SUB_CHANNEL_A, 0, true)
        CAPTAINS_LOG_CHANNEL(RENDER_SUB_CHANNEL_A_VERBOSE, 5, true)
        CAPTAINS_LOG_CHANNEL(RENDER_SUB_CHANNEL_B, 2, true)
      CAPTAINS_LOG_CHANNEL_END_CHILDREN()
    CAPTAINS_LOG_CHANNEL(NETWORK, 0, true)
    CAPTAINS_LOG_CHANNEL(AUDIO, 0, true)
      CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN()
        CAPTAINS_LOG_CHANNEL(AUDIO_SUB_CHANNEL_Z, 0, true)
        CAPTAINS_LOG_CHANNEL(AUDIO_SUB_CHANNEL_OTHER, 2, true)
        CAPTAINS_LOG_CHANNEL(AUDIO_IO, 3, true)
      CAPTAINS_LOG_CHANNEL_END_CHILDREN()
    CAPTAINS_LOG_CHANNEL(MISC, 0, true)
    CAPTAINS_LOG_CHANNEL(SET_THIS_EXAMPLE, 0, true)
    CAPTAINS_LOG_CHANNEL(SET_ADDRESS_EXAMPLE, 0, true)
    CAPTAINS_LOG_CHANNEL(SET_STORE_NAME_EXAMPLE, 0, true)
    CAPTAINS_LOG_CHANNEL(LAMBDA_EXAMPLE, 0, true)
    CAPTAINS_LOG_CHANNEL(DEFAULT, 0, true)
  CAPTAINS_LOG_CHANNEL_END_CHILDREN()

