// Do not at pragma or header guard.

/**
 * Channels are defined using the X-Macros technique in a tree configuration.
 * Example of X-Macro trees can be found here: [danilafe.com/blog/chapel_x_macros/](https://danilafe.com/blog/chapel_x_macros/)
 *
 * CAPTAINS_LOG_CHANNEL(name, verbosityLevel, executionLevel):
 * param name: The name of the channel. Must be unique.
 * param verbosityLevel: larger the number, the more verbose it is.  (currently unused)
 * param executionLevel: one of the following enum values:
 *      - FULLY_ENABLED
 *      - ENABLED_NO_OUTPUT
 *      - FULLY_DISABLED
 */

CAPTAINS_LOG_CHANNEL(ALL, 0, FULLY_ENABLED)
  CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN()
    CAPTAINS_LOG_CHANNEL(RENDER, 0, FULLY_ENABLED)
      CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN()
        CAPTAINS_LOG_CHANNEL(RENDER_SUB_CHANNEL_A, 0, FULLY_ENABLED)
        CAPTAINS_LOG_CHANNEL(RENDER_SUB_CHANNEL_A_VERBOSE, 5, FULLY_ENABLED)
        CAPTAINS_LOG_CHANNEL(RENDER_SUB_CHANNEL_B, 2, FULLY_ENABLED)
      CAPTAINS_LOG_CHANNEL_END_CHILDREN()
    CAPTAINS_LOG_CHANNEL(NETWORK, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(AUDIO, 0, FULLY_ENABLED)
      CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN()
        CAPTAINS_LOG_CHANNEL(AUDIO_SUB_CHANNEL_Z, 0, FULLY_ENABLED)
        CAPTAINS_LOG_CHANNEL(AUDIO_SUB_CHANNEL_OTHER, 2, FULLY_ENABLED)
        CAPTAINS_LOG_CHANNEL(AUDIO_IO, 3, FULLY_ENABLED)
      CAPTAINS_LOG_CHANNEL_END_CHILDREN()
    CAPTAINS_LOG_CHANNEL(MISC, 0,FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(SET_THIS_EXAMPLE, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(SET_ADDRESS_EXAMPLE, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(SET_STORE_NAME_EXAMPLE, 0, ENABLED_NO_OUTPUT)
    CAPTAINS_LOG_CHANNEL(LAMBDA_EXAMPLE, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(ENABLED_CHANNEL, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(NO_OUTPUT_CHANNEL, 0, ENABLED_NO_OUTPUT)
    CAPTAINS_LOG_CHANNEL(DISABLED_CHANNEL, 0, FULLY_DISABLED)
      CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN()
        CAPTAINS_LOG_CHANNEL(CHANNEL_ENABLED_BUT_SHOULD_BE_DISABLED_IN_LOG, 0, ENABLED_NO_OUTPUT)
          CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN()
            CAPTAINS_LOG_CHANNEL(CHANNEL_ENABLED_BUT_SHOULD_BE_DISABLED_IN_LOG_2, 0, FULLY_ENABLED)
          CAPTAINS_LOG_CHANNEL_END_CHILDREN()
      CAPTAINS_LOG_CHANNEL_END_CHILDREN()
    CAPTAINS_LOG_CHANNEL(NESTED_CHANNEL, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(DEFAULT, 0, FULLY_ENABLED)
  CAPTAINS_LOG_CHANNEL_END_CHILDREN()

