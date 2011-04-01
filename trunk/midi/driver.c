#include <stdlib.h>
#include "runloop.h"
#define MIDI_DRIVER_INTERNALS
#include "driver.h"
#include "clock.h"
#include "connector.h"
#include "list.h"
#include "message.h"
#include "port.h"

/**
 * @defgroup MIDI-driver MIDI driver implementations
 * @ingroup MIDI
 * Implementations of the @ref MIDIDriver interface.
 */

/**
 * @ingroup MIDI
 * @def MIDI_DRIVER_DELEGATE_INITIALIZER
 * @brief Initializer for MIDI driver delegates.
 * Assign to a delegate to initialize as empty.
 */

/**
 * @ingroup MIDI
 * @def MIDI_DRIVER_WILL_SEND_MESSAGE
 * @brief MIDI driver will send a message.
 * This is called by the driver interface before it will send a message to
 * the implementation. Return a value other than 0 to cancel the sending.
 * The info pointer points the the message that will be sent.
 */
/**
 * @ingroup MIDI
 * @def MIDI_DRIVER_WILL_RECEIVE_MESSAGE
 * @brief MIDI driver will receive a message.
 * This is called by the driver interface after it has been notified
 * (by the implementation) that a new message was received. The interface
 * will then deliver it to all connected devices unless the callback returned
 * a value other than 0. The info pointer points to the message that will
 * be received.
 */

/**
 * @ingroup MIDI
 * @struct MIDIDriverDelegate driver.h
 * @brief Delegate for driver communication.
 * Delegate for bi-directional communication between a MIDIDriver and
 * it's implementation using MIDIMessages.
 */
/**
 * @public @property MIDIDriverDelegate::send
 * @brief Callback for sending.
 * This callback is called by the driver when it wants to send a @c MIDIMessage
 * with the implementation. The @c implementation element is passed as first parameter.
 * @param implementation The implementation pointer given to the delegate.
 * @param message        The message that will be sent.
 */
/**
 * @public @property MIDIDriverDelegate::receive
 * @brief Callback for receiving.
 * This callback can be called by the implementation when it wants to notify the
 * driver interface of incoming messages. The @c interface element has to be passed as
 * first parameter.
 * @param interface The interface pointer given to the delegate.
 * @param message   The message that was received.
 */
/**
 * @public @property MIDIDriverDelegate::event
 * @brief Callback for various state changes or events.
 * This is called in various places and it's semantics depend on the
 * event that happened. In general, you should only respond to events
 * you know.
 * @param observer       The observer that handles the events.
 * @param interface      The interface pointer given to the delegate.
 * @param implementation The implementation pointer given to the delegate.
 * @param event          An event number.
 * @param info           Ancillary information specified by the event type.
 */
/**
 * @public @property MIDIDriverDelegate::implementation
 * @brief The driver implementation.
 * This should point to a valid driver implementation object, for example a
 * MIDIDriverAppleMIDI, MIDIDriverCoreMIDI or MIDIDriverOSC object.
 */
/**
 * @public @property MIDIDriverDelegate::interface
 * @brief The driver interface.
 * This will be set by the MIDIDriver interface to point to the MIDIDriver.
 */
/**
 * @public @property MIDIDriverDelegate::observer
 * @brief The observer that manages event and status changes.
 */

/**
 * @brief Send a MIDIMessage.
 * @public @memberof MIDIDriverDelegate
 * @param delegate The delegate.
 * @param message  The message.
 */
/*int MIDIDriverDelegateSendMessage( struct MIDIDriverDelegate * delegate, struct MIDIMessage * message ) {
  if( delegate == NULL || delegate->send == NULL ) return 0;
  return (*delegate->send)( delegate->implementation, message );
}
*/

/**
 * @brief Receive a MIDIMessage.
 * @public @memberof MIDIDriverDelegate
 * @param delegate The delegate.
 * @param message  The message.
 */
/*int MIDIDriverDelegateReceiveMessage( struct MIDIDriverDelegate * delegate, struct MIDIMessage * message ) {
  if( delegate == NULL || delegate->receive == NULL ) return 0;
  return (*delegate->receive)( delegate->interface, message );
}
*/

/**
 * @brief Trigger any event.
 * @public @memberof MIDIDriverDelegate
 * @param delegate The delegate.
 * @param event    The event.
 * @param info     Ancillary info.
 */
/*int MIDIDriverDelegateTriggerEvent( struct MIDIDriverDelegate * delegate, int event, void * info ) {
  if( delegate == NULL || delegate->event == NULL ) return 0;
  return (*delegate->event)( delegate->observer, delegate->interface, delegate->implementation,
                             event, info );
}
*/

/**
 * @ingroup MIDI
 * @brief Interface to send MIDI messages with various drivers.
 * The MIDIDriver is an interface that can be used to pass messages
 * to an underlying implementation. The communication between the interface
 * and it's implementation is estrablished using the delegate that is passed
 * on initialization.
 */

#pragma mark Connector list management
/**
 * @internal
 * Connector list management.
 * @{
 */

static void _detach_source_and_release( void * connector ) {
  MIDIConnectorDetachSource( connector );
  MIDIConnectorRelease( connector );
}

static void _detach_target_and_release( void * connector ) {
  MIDIConnectorDetachTarget( connector );
  MIDIConnectorRelease( connector );
}

static int _receiver_connect( void * driverp, struct MIDIConnector * receiver ) {
  struct MIDIDriver * driver = driverp;
  MIDIListAdd( driver->receivers, receiver );
  return 0;
}

static int _receiver_disconnect( void * driverp, struct MIDIConnector * receiver ) {
  struct MIDIDriver * driver = driverp;
  MIDIListRemove( driver->receivers, receiver );
  return 0;
}

static int _sender_relay( void * driverp, struct MIDIMessage * message ) {
  return MIDIDriverSend( driverp, message );
}

static int _sender_connect( void * driverp, struct MIDIConnector * sender ) {
  struct MIDIDriver * driver = driverp;
  MIDIListAdd( driver->senders, sender );
  return 0;
}

static int _sender_disconnect( void * driverp, struct MIDIConnector * sender ) {
  struct MIDIDriver * driver = driverp;
  MIDIListRemove( driver->senders, sender );
  return 0;
}

static int _driver_receive( void * driverp, struct MIDIMessage * message ) {
  return MIDIDriverReceive( driverp, message );
}

/** @} */

#pragma mark Creation and destruction
/**
 * @name Creation and destruction
 * Creating, destroying and reference counting of MIDIDriver objects.
 * @{
 */
 
static int _port_receive( void * target, void * source, int type, size_t size, void * data ) {
  struct MIDIDriver * driver = target;
  /** @todo: check for correct message type */
  if( type == 0 ) {
    return MIDIDriverReceive( driver, data );
  } else {
    return 0;
  }
}

struct MIDIDriver * MIDIDriverCreate( char * name, MIDISamplingRate rate ) {
  struct MIDIDriver * driver = malloc( sizeof( struct MIDIDriver ) );
  MIDIPrecondReturn( driver != NULL, ENOMEM, NULL );
  MIDIDriverInit( driver, name, rate );
  return driver;
}

/**
 * @brief Create a MIDIDriver instance.
 * Allocate space and initialize a MIDIDriver instance.
 * @public @memberof MIDIDriver
 * @param delegate The delegate to use for the driver. May be @c NULL.
 * @return a pointer to the created driver structure on success.
 * @return a @c NULL pointer if the driver could not created.
 */
void MIDIDriverInit( struct MIDIDriver * driver, char * name, MIDISamplingRate rate ) {
  MIDIPrecondReturn( driver != NULL, EFAULT, (void)0 );
  MIDISamplingRate global_rate;

  driver->refs = 1;
/*driver->delegate  = delegate;*/
  driver->receivers = MIDIListCreate( (MIDIRefFn*) &MIDIConnectorRetain, (MIDIRefFn*) &_detach_source_and_release );
  driver->senders   = MIDIListCreate( (MIDIRefFn*) &MIDIConnectorRetain, (MIDIRefFn*) &_detach_target_and_release );
  driver->port      = MIDIPortCreate( name, MIDI_PORT_RECEIVE | MIDI_PORT_SEND, driver, &_port_receive );

  MIDIClockGetGlobalClock( &(driver->clock) );
  MIDIClockGetSamplingRate( driver->clock, &global_rate );

  if( global_rate == rate ) {
    MIDIClockRetain( driver->clock );
  } else {
    driver->clock = MIDIClockCreate( rate );
  }

  driver->send    = NULL;
  driver->destroy = NULL;

/*
  if( delegate != NULL ) {
    delegate->receive   = &_driver_receive;
    delegate->interface = driver;
  }
  return driver;*/
}

/**
 * @brief Destroy a MIDIDriver instance.
 * Free all resources occupied by the driver and release all referenced objects.
 * @public @memberof MIDIDriver
 * @param driver The driver.
 */
void MIDIDriverDestroy( struct MIDIDriver * driver ) {
  MIDIPrecondReturn( driver != NULL, EFAULT, (void)0 );
  if( driver->destroy != NULL ) {
    (*driver->destroy)( driver );
  }
  if( driver->clock != NULL ) {
    MIDIClockRelease( driver->clock );
  }
  MIDIPortRelease( driver->port );
  MIDIListRelease( driver->receivers );
  MIDIListRelease( driver->senders );
/*driver->delegate->receive   = NULL;
  driver->delegate->interface = NULL;*/
  free( driver );
}

/**
 * @brief Retain a MIDIDriver instance.
 * Increment the reference counter of a driver so that it won't be destroyed.
 * @public @memberof MIDIDriver
 * @param driver The driver.
 */
void MIDIDriverRetain( struct MIDIDriver * driver ) {
  MIDIPrecondReturn( driver != NULL, EFAULT, (void)0 );
  driver->refs++;
}

/**
 * @brief Release a MIDIDriver instance.
 * Decrement the reference counter of a driver. If the reference count
 * reached zero, destroy the driver.
 * @public @memberof MIDIDriver
 * @param driver The driver.
 */
void MIDIDriverRelease( struct MIDIDriver * driver ) {
  MIDIPrecondReturn( driver != NULL, EFAULT, (void)0 );
  if( ! --driver->refs ) {
    MIDIDriverDestroy( driver );
  }
}

/** @} */

#pragma mark Connector attachment
/**
 * @name Connector attachment
 * Methods to obtain connectors that are attached to the driver.
 * @{
 */

/**
 * @brief Delegate for receiving from a driver.
 * @relatesalso MIDIDriver
 * @see         MIDIConnector
 */
struct MIDIConnectorSourceDelegate MIDIDriverReceiveConnectorDelegate = {
  &_receiver_connect,
  &_receiver_disconnect
};

/**
 * @brief Delegate for sending through driver.
 * @relatesalso MIDIDriver
 * @see         MIDIConnector
 */
struct MIDIConnectorTargetDelegate MIDIDriverSendConnectorDelegate = {
  &_sender_relay,
  &_sender_connect,
  &_sender_disconnect
};

/**
 * @brief Get the input port.
 * @public @memberof MIDIDriver
 * @param driver The driver.
 * @param port   The port.
 * @retval 0 on success.
 */
int MIDIDriverGetInputPort( struct MIDIDriver * driver, struct MIDIPort ** port ) {
  *port = driver->port;
  return 0;
}

/**
 * @brief Get the output port.
 * @public @memberof MIDIDriver
 * @param driver The driver.
 * @param port   The port.
 * @retval 0 on success.
 */
int MIDIDriverGetOutputPort( struct MIDIDriver * driver, struct MIDIPort ** port ) {
  *port = driver->port;
  return 0;
}

/**
 * @brief Provice a connector for sending MIDI data.
 * Provide a connector that can be used to send MIDI messages
 * using the driver.
 * The connector that is stored in @c send will have a retain count
 * of one and should only be released by the user if it was retained
 * before.
 * @public @memberof MIDIDriver
 * @param driver The driver.
 * @param send   The location to store the pointer to the connector in.
 * @retval 0  on success.
 * @retval >0 if the connector could not be provided.
 */
int MIDIDriverProvideSendConnector( struct MIDIDriver * driver, struct MIDIConnector ** send ) {
  struct MIDIConnector * connector;
  if( send == NULL ) return 1;
  connector = MIDIConnectorCreate();
  if( connector == NULL ) return 1;
  MIDIConnectorAttachToDriver( connector, driver );
  MIDIListAdd( driver->senders, connector );
  *send = connector;
  MIDIConnectorRelease( connector ); /* retained by list */
  return 0;
}

/**
 * @brief Provice a connector for receiving MIDI data.
 * Provide a connector that can be used to receive MIDI messages
 * using the driver.
 * The connector that is stored in @c receive will have a retain count
 * of one and should only be released by the user if it was retained
 * before.
 * @public @memberof MIDIDriver
 * @param driver  The driver.
 * @param receive The location to store the pointer to the connector in.
 * @retval 0  on success.
 * @retval >0 if the connector could not be provided.
 */
int MIDIDriverProvideReceiveConnector( struct MIDIDriver * driver, struct MIDIConnector ** receive ) {
  struct MIDIConnector * connector;
  if( receive == NULL ) return 1;
  connector = MIDIConnectorCreate();
  if( connector == NULL ) return 1;
  MIDIConnectorAttachFromDriver( connector, driver );
  MIDIListAdd( driver->receivers, connector );
  *receive = connector;
  MIDIConnectorRelease( connector ); /* retained by list */
  return 0;
}

/** @} */

#pragma mark Message passing
/**
 * @name Message passing
 * Receiving and sending MIDIMessage objects.
 * @{
 */

/**
 * @brief Make the MIDIDriver implement itself as loopback.
 * The driver's delegate will be modified so that it passes
 * outgoing messages to it's own receive method.
 * @public @memberof MIDIDriver
 * @param driver The driver
 * @retval 0  on success.
 * @retval >0 if the operation could not be completed.
 */
int MIDIDriverMakeLoopback( struct MIDIDriver * driver ) {
/*if( driver->delegate == NULL ) return 1;
  driver->delegate->send = &_driver_receive;
  driver->delegate->implementation = driver;*/
  return 0;
}

/**
 * @brief Receive a generic MIDIMessage.
 * Relay an incoming message via all attached receiving connectors.
 * This should be called by the driver delegate whenever
 * a new message was received.
 * @public @memberof MIDIDriver
 * @param driver  The driver.
 * @param message The message.
 * @retval 0  on success.
 * @retval >0 if the message could not be relayed.
 */
int MIDIDriverReceive( struct MIDIDriver * driver, struct MIDIMessage * message ) {
  MIDIPrecond( driver != NULL, EFAULT );
  MIDIPrecond( message != NULL, EINVAL );
/*if( MIDIDriverDelegateTriggerEvent( driver->delegate, MIDI_DRIVER_WILL_RECEIVE_MESSAGE, message ) ) return 1;*/
  size_t size;
  MIDIMessageGetSize( message, &size );
  return MIDIPortSend( driver->port, 0, size, message );
}

/**
 * @brief Send a generic MIDIMessage.
 * Pass an outgoing message to the driver delegate.
 * The delegate should take care of sending.
 * @public @memberof MIDIDriver
 * @param driver  The driver.
 * @param message The message.
 * @retval 0  on success.
 * @retval >0 if the message could not be sent.
 */
int MIDIDriverSend( struct MIDIDriver * driver, struct MIDIMessage * message ) {
  MIDIPrecond( driver != NULL, EFAULT );
  MIDIPrecond( message != NULL, EINVAL );
/*if( MIDIDriverDelegateTriggerEvent( driver->delegate, MIDI_DRIVER_WILL_SEND_MESSAGE, message ) ) return 1;
  return MIDIDriverDelegateSendMessage( driver->delegate, message );*/
  if( driver->send == NULL ) return 1;
  return (*driver->send)( driver, message );
}

int MIDIDriverTriggerEvent( struct MIDIDriver * driver, int type, size_t size, void * data ) {
  MIDIPrecond( driver != NULL, EFAULT );
  return MIDIPortSend( driver->port, type, size, data );
}

/** @} */
