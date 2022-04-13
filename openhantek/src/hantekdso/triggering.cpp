// SPDX-License-Identifier: GPL-2.0+

#include <QDebug>
#include <cmath>
#include "hantekdsocontrol.h"

// search for trigger point from defined point, default startPos = 0;
// return trigger position > 0 (0: no trigger found)
int HantekDsoControl::searchTriggerPoint( Dso::Slope dsoSlope, int startPos ) {
    int slope;
    if ( dsoSlope == Dso::Slope::Positive )
        slope = 1;
    else if ( dsoSlope == Dso::Slope::Negative )
        slope = -1;
    else
        return 0;

    unsigned channel = unsigned( controlsettings.trigger.source );
    const std::vector< double > &samples = result.data[ channel ];
    int sampleCount = int( samples.size() ); ///< number of available samples
    if ( startPos < 0 || startPos >= sampleCount )
        return 0;
    double triggerLevel = controlsettings.trigger.level[ channel ];
    if ( verboseLevel > 5 )
        qDebug() << "     HDC::searchTriggerPoint()" << channel << triggerLevel << slope << startPos;
    int samplesDisplay = int( round( controlsettings.samplerate.target.duration * controlsettings.samplerate.current ) );
    int searchBegin;
    int searchEnd;
    if ( 0 == startPos ) {                                                      // search 1st trigger slope
        searchBegin = int( controlsettings.trigger.position * samplesDisplay ); // samples left of trigger
        searchEnd = sampleCount - ( int( samplesDisplay ) - searchBegin );      // samples right of trigger
    } else {                                                                    // search next slopes for duty cycle
        searchBegin = startPos;                                                 // search from start point ..
        searchEnd = sampleCount;                                                // .. up to end of samples
    }
    // Two possible search scenarios:
    // 1. search for the trigger slope that allows stable trace display (omit pre and post trigger area)
    // |-----------samples-----------| // available sample
    // |--disp--|                      // display size
    // |<<<<<T>>|--------------------| // >> = right = (disp-pre) i.e. right of trigger on screen
    // |<pre<|                         // << = left = pre
    // |--(samp-(disp-pre))-------|>>|
    // |<<<<<|????????????????????|>>| // ?? = search for trigger in this range [left,right]
    // 2. search duty cycle slopes without need for stable display margins
    // |<<<<<T???????????????????????| // ?? = search for other (duty cycle) slopes in this range

    const int triggerAverage = int( pow( 20, controlsettings.trigger.smooth ) ); // smooth 0,1,2 -> 1,20,400
    if ( searchBegin < triggerAverage )
        searchBegin = triggerAverage;
    if ( searchEnd >= sampleCount - triggerAverage )
        searchEnd = sampleCount - triggerAverage - 1;
    if ( verboseLevel > 5 )
        qDebug() << "     begin:" << searchBegin << "end:" << searchEnd;

    double prev = INT_MAX;
    int swTriggerStart = 0;
    for ( int i = searchBegin; i < searchEnd; i++ ) {
        if ( slope * samples[ size_t( i ) ] >= slope * triggerLevel &&
             slope * prev < slope * triggerLevel ) { // trigger condition met
            // check for the previous few SampleSet samples, if they are also above/below the trigger value
            // use different averaging sizes for HF, normal and LF signals
            bool triggerBefore = false;
            double mean = 0;
            int iii = 0;
            for ( int k = i - 1; k >= i - triggerAverage && k >= 0; k-- ) {
                mean += samples[ size_t( k ) ];
                iii++;
            }
            if ( iii ) {
                mean /= iii;
                triggerBefore = slope * mean < slope * triggerLevel;
            }
            // check for the next few SampleSet samples, if they are also above/below the trigger value
            bool triggerAfter = false;
            if ( triggerBefore ) { // search right side only if left side condition is met
                mean = 0;
                iii = 0;
                for ( int k = i + 1; k <= i + triggerAverage && k < sampleCount; k++ ) {
                    mean += samples[ size_t( k ) ];
                    iii++;
                }
                if ( iii ) {
                    mean /= iii;
                    triggerAfter = slope * mean > slope * triggerLevel;
                }
            }
            // if at least triggerAverage samples before and after trig meet the condition, set trigger
            if ( triggerBefore && triggerAfter ) {
                swTriggerStart = i;
                break;
            }
        }
        prev = samples[ size_t( i ) ];
    }
    if ( verboseLevel > 5 )
        qDebug() << "     swT:" << swTriggerStart;
    return swTriggerStart;
} // searchTriggerPoint()


int HantekDsoControl::searchTriggeredPosition() {
    static Dso::Slope nextSlope = Dso::Slope::Positive; // for alternating slope mode X
    ChannelID channel = ChannelID( controlsettings.trigger.source );
    // Trigger channel not in use
    if ( !scope->anyUsed( channel ) || result.data.empty() || result.data[ channel ].empty() )
        return result.triggeredPosition = 0;
    if ( verboseLevel > 4 )
        qDebug() << "    HDC::searchTriggeredPosition()" << result.tag;
    triggeredPositionRaw = 0;
    double pulseWidth1 = 0.0;
    double pulseWidth2 = 0.0;

    size_t sampleCount = result.data[ channel ].size();              // number of available samples
    double timeDisplay = controlsettings.samplerate.target.duration; // time for full screen width
    double sampleRate = result.samplerate;                           //
    unsigned samplesDisplay = unsigned( round( timeDisplay * controlsettings.samplerate.current ) );
    if ( sampleCount < samplesDisplay ) // not enough samples to adjust for jitter.
        return result.triggeredPosition = 0;

    // search for trigger point in a range that leaves enough samples left and right of trigger for display
    // find also up to two alternating slopes after trigger point -> calculate pulse widths and duty cycle.
    if ( controlsettings.trigger.slope != Dso::Slope::Both ) // up or down
        nextSlope = controlsettings.trigger.slope;           // use this slope

    triggeredPositionRaw = searchTriggerPoint( nextSlope ); // get 1st slope position
    if ( triggeredPositionRaw ) { // triggered -> search also following other slope (calculate pulse width)
        if ( int slopePos2 = searchTriggerPoint( mirrorSlope( nextSlope ), triggeredPositionRaw ) ) {
            pulseWidth1 = ( slopePos2 - triggeredPositionRaw ) / sampleRate;
            if ( int slopePos3 = searchTriggerPoint( nextSlope, slopePos2 ) ) { // search 3rd slope
                pulseWidth2 = ( slopePos3 - slopePos2 ) / sampleRate;
            }
        }
        if ( controlsettings.trigger.slope == Dso::Slope::Both ) // trigger found and alternating?
            nextSlope = mirrorSlope( nextSlope );                // use opposite direction next time
    }

    result.triggeredPosition = triggeredPositionRaw; // align trace to trigger position
    result.pulseWidth1 = pulseWidth1;
    result.pulseWidth2 = pulseWidth2;
    if ( verboseLevel > 5 ) // HACK: This assumes that positive=0 and negative=1
        qDebug() << "     nextSlope:"
                 << "/\\"[ int( nextSlope ) ] << "triggeredPositionRaw:" << triggeredPositionRaw;
    return result.triggeredPosition;
} // searchTriggeredPosition()


bool HantekDsoControl::provideTriggeredData() {
    if ( verboseLevel > 4 )
        qDebug() << "    HDC::provideTriggeredData()" << result.tag;
    static DSOsamples triggeredResult; // storage for last triggered trace samples
    if ( result.triggeredPosition ) {  // live trace has triggered
        // Use this trace and save it also
        triggeredResult.data = result.data;
        triggeredResult.samplerate = result.samplerate;
        triggeredResult.clipped = result.clipped;
        triggeredResult.triggeredPosition = result.triggeredPosition;
        result.liveTrigger = true;
    } else if ( controlsettings.trigger.mode == Dso::TriggerMode::NORMAL ) { // Not triggered in NORMAL mode
        // Use saved trace (even if it is empty)
        result.data = triggeredResult.data;
        result.samplerate = triggeredResult.samplerate;
        result.clipped = triggeredResult.clipped;
        result.triggeredPosition = triggeredResult.triggeredPosition;
        result.liveTrigger = false; // show red "TR" top left
    } else {                        // Not triggered and not NORMAL mode
        // Use the free running trace, discard history
        triggeredResult.data.clear();          // discard trace
        triggeredResult.triggeredPosition = 0; // not triggered
        result.liveTrigger = false;            // show red "TR" top left
    }
    return result.liveTrigger;
} // bool HantekDsoControl::provideTriggeredData()


Dso::ErrorCode HantekDsoControl::setTriggerMode( Dso::TriggerMode mode ) {
    if ( deviceNotConnected() )
        return Dso::ErrorCode::CONNECTION;

    if ( verboseLevel > 2 )
        qDebug() << "  HDC::setTriggerMode()" << int( mode );
    static Dso::TriggerMode lastMode;
    controlsettings.trigger.mode = mode;
    if ( Dso::TriggerMode::SINGLE != mode )
        enableSampling();
    // trigger mode changed NONE <-> !NONE
    if ( ( Dso::TriggerMode::ROLL == mode && Dso::TriggerMode::ROLL != lastMode ) ||
         ( Dso::TriggerMode::ROLL != mode && Dso::TriggerMode::ROLL == lastMode ) ) {
        restartSampling(); // invalidate old samples
        raw.freeRun = Dso::TriggerMode::ROLL == mode;
    }
    lastMode = mode;
    requestRefresh();
    return Dso::ErrorCode::NONE;
}


Dso::ErrorCode HantekDsoControl::setTriggerSource( int channel ) {
    if ( deviceNotConnected() )
        return Dso::ErrorCode::CONNECTION;
    if ( verboseLevel > 2 )
        qDebug() << "  HDC::setTriggerSource()" << channel;
    controlsettings.trigger.source = channel;
    requestRefresh();
    return Dso::ErrorCode::NONE;
}


Dso::ErrorCode HantekDsoControl::setTriggerSmooth( int smooth ) {
    if ( deviceNotConnected() )
        return Dso::ErrorCode::CONNECTION;
    if ( verboseLevel > 2 )
        qDebug() << "  HDC::setTriggerSmooth()" << smooth;
    controlsettings.trigger.smooth = smooth;
    requestRefresh();
    return Dso::ErrorCode::NONE;
}


// trigger level in Volt
Dso::ErrorCode HantekDsoControl::setTriggerLevel( ChannelID channel, double level ) {
    if ( deviceNotConnected() )
        return Dso::ErrorCode::CONNECTION;
    if ( channel > specification->channels )
        return Dso::ErrorCode::PARAMETER;
    if ( verboseLevel > 2 )
        qDebug() << "  HDC::setTriggerLevel()" << channel << level;
    controlsettings.trigger.level[ channel ] = level;
    requestRefresh();
    displayInterval = 0; // update screen immediately
    return Dso::ErrorCode::NONE;
}


Dso::ErrorCode HantekDsoControl::setTriggerSlope( Dso::Slope slope ) {
    if ( deviceNotConnected() )
        return Dso::ErrorCode::CONNECTION;
    if ( verboseLevel > 2 )
        qDebug() << "  HDC::setTriggerSlope()" << int( slope );
    controlsettings.trigger.slope = slope;
    requestRefresh();
    return Dso::ErrorCode::NONE;
}


// set trigger position (0.0 - 1.0)
Dso::ErrorCode HantekDsoControl::setTriggerPosition( double position ) {
    if ( deviceNotConnected() )
        return Dso::ErrorCode::CONNECTION;
    if ( verboseLevel > 2 )
        qDebug() << "  HDC::setTriggerPosition()" << position;
    controlsettings.trigger.position = position;
    requestRefresh();
    return Dso::ErrorCode::NONE;
}

