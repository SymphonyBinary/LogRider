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
    CAPTAINS_LOG_CHANNEL(main, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(processLogLineCharLimit, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(processChannelLine, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(processLogLine, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(processBlockInnerLine, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(processBlockScopeClose, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(processBlockScopeOpen, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(processIncompleteLineContinue, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(processIncompleteLineBegin, 0, FULLY_ENABLED)
    CAPTAINS_LOG_CHANNEL(stackNode, 0, FULLY_ENABLED)
  CAPTAINS_LOG_CHANNEL_END_CHILDREN()