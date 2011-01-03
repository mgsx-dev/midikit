#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "applemidi.h"
#include "driver/common/rtp.h"
#include "driver/common/rtpmidi.h"
#include "midi/message_queue.h"

#define APPLEMIDI_PROTOCOL_SIGNATURE          0xffff

#define APPLEMIDI_COMMAND_INVITATION          0x494e /** "IN" */
#define APPLEMIDI_COMMAND_INVITATION_REJECTED 0x4e4f /** "NO" */
#define APPLEMIDI_COMMAND_INVITATION_ACCEPTED 0x4f4b /** "OK" */
#define APPLEMIDI_COMMAND_ENDSESSION          0x4259 /** "BY" */
#define APPLEMIDI_COMMAND_SYNCHRONIZATION     0x434b /** "CK" */
#define APPLEMIDI_COMMAND_RECEIVER_FEEDBACK   0x5253 /** "RS" */

struct AppleMIDICommand {
  struct RTPPeer * peer; /* use peers sockaddr instead .. we get initialization problems otherwise */
  struct sockaddr_storage addr;
  socklen_t size;
  int type;
  union {
    struct {
      unsigned long version;
      unsigned long token;
      unsigned long ssrc;
      char name[16];
    } session;
    struct {
      unsigned long ssrc;
      unsigned long count;
      unsigned long timestamp1;
      unsigned long timestamp2;
      unsigned long timestamp3;
    } sync;
    struct {
      unsigned long ssrc;
      unsigned long seqnum;
    } feedback;
  } data;
};

struct MIDIDriverAppleMIDI {
  size_t refs;
  int control_socket;
  int rtp_socket;
  unsigned short port;
  unsigned long  token;

  struct RTPSession * rtp_session;
  struct RTPMIDISession * rtpmidi_session;
  
  struct MIDIMessageQueue * in_queue;
  struct MIDIMessageQueue * out_queue;
};

struct MIDIDriverDelegate MIDIDriverDelegateAppleMIDI = {
  NULL
};

static int _applemidi_connect( struct MIDIDriverAppleMIDI * driver ) {
  struct sockaddr_in addr;

  if( driver->control_socket <= 0 ) {
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons( driver->port );

    driver->control_socket = socket( PF_INET, SOCK_DGRAM, 0 );
    bind( driver->control_socket, (struct sockaddr *) &addr, sizeof(addr) );
  }

  if( driver->rtp_socket <= 0 ) {
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons( driver->port + 1 );

    driver->rtp_socket = socket( PF_INET, SOCK_DGRAM, 0 );
    bind( driver->rtp_socket, (struct sockaddr *) &addr, sizeof(addr) );
  }
  return 0;
}

static int _applemidi_disconnect( struct MIDIDriverAppleMIDI * driver, int fd ) {
  if( fd == driver->control_socket || fd == 0 ) {
    if( driver->control_socket > 0 ) {
      if( close( driver->control_socket ) ) {
        return 1;
      }
      driver->control_socket = 0;
    }
  }

  if( fd == driver->control_socket || fd == 0 ) {
    if( driver->rtp_socket > 0 ) {
      if( close( driver->rtp_socket ) ) {
        return 1;
      }
      driver->rtp_socket = 0;
    }
  }
  return 0;
}

/**
 * @brief Create a MIDIDriverAppleMIDI instance.
 * Allocate space and initialize an MIDIDriverAppleMIDI instance.
 * @public @memberof MIDIDriverAppleMIDI
 * @return a pointer to the created driver structure on success.
 * @return a @c NULL pointer if the driver could not created.
 */
struct MIDIDriverAppleMIDI * MIDIDriverAppleMIDICreate() {
  struct MIDIDriverAppleMIDI * driver;

  driver = malloc( sizeof( struct MIDIDriverAppleMIDI ) );
  if( driver == NULL ) return NULL;
  
  driver->refs = 1;
  driver->control_socket = 0;
  driver->rtp_socket     = 0;
  driver->port           = 5004;
  
  RTPSessionGetTimestamp( driver->rtp_session, &(driver->token) );

  _applemidi_connect( driver );

  driver->rtp_session     = RTPSessionCreate( driver->rtp_socket );  
  driver->rtpmidi_session = RTPMIDISessionCreate( driver->rtp_session );
  driver->in_queue  = MIDIMessageQueueCreate();
  driver->out_queue = MIDIMessageQueueCreate();

  RTPSessionSetTimestampRate( driver->rtp_session, 44100.0 );

  return driver;
}

/**
 * @brief Destroy a MIDIDriverAppleMIDI instance.
 * Free all resources occupied by the driver.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 */
void MIDIDriverAppleMIDIDestroy( struct MIDIDriverAppleMIDI * driver ) {
  RTPMIDISessionRelease( driver->rtpmidi_session );
  RTPSessionRelease( driver->rtp_session );
  MIDIMessageQueueRelease( driver->in_queue );
  MIDIMessageQueueRelease( driver->out_queue );
  _applemidi_disconnect( driver, 0 );
}

/**
 * @brief Retain a MIDIDriverAppleMIDI instance.
 * Increment the reference counter of a driver so that it won't be destroyed.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 */
void MIDIDriverAppleMIDIRetain( struct MIDIDriverAppleMIDI * driver ) {
  driver->refs++;
}

/**
 * @brief Release a MIDIDriverAppleMIDI instance.
 * Decrement the reference counter of a driver. If the reference count
 * reached zero, destroy the driver.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 */
void MIDIDriverAppleMIDIRelease( struct MIDIDriverAppleMIDI * driver ) {
  if( ! --driver->refs ) {
    MIDIDriverAppleMIDIDestroy( driver );
  }
}

/**
 * @brief Set the base port to be used for session management.
 * The RTP port will be the control port plus one.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param port The port.
 * @retval 0 On success.
 * @retval >0 If the port could not be set.
 */
int MIDIDriverAppleMIDISetPort( struct MIDIDriverAppleMIDI * driver, unsigned short port ) {
  if( port == driver->port ) return 0;
  /* reconnect if connected? */
  driver->port = port;
  return 0;
}

/**
 * @brief Get the port used for session management.
 * The RTP port can be computed by adding one.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param port The port.
 * @retval 0 On success.
 * @retval >0 If the port could not be set.
 */
int MIDIDriverAppleMIDIGetPort( struct MIDIDriverAppleMIDI * driver, unsigned short * port ) {
  if( port == NULL ) return 1;
  *port = driver->port;
  return 0;
}

int MIDIDriverAppleMIDISetRTPSocket( struct MIDIDriverAppleMIDI * driver, int socket ) {
  if( socket == driver->rtp_socket ) return 0;
  int result = _applemidi_disconnect( driver, driver->rtp_socket );
  if( result == 0 ) {
    driver->rtp_socket = socket;
  }
  return result;
}

int MIDIDriverAppleMIDIGetRTPSocket( struct MIDIDriverAppleMIDI * driver, int * socket ) {
  *socket = driver->rtp_socket;
  return 0;
}

int MIDIDriverAppleMIDISetControlSocket( struct MIDIDriverAppleMIDI * driver, int socket ) {
  if( socket == driver->control_socket ) return 0;
  int result = _applemidi_disconnect( driver, driver->control_socket );
  if( result == 0 ) {
    driver->control_socket = socket;
  }
  return result;
}

int MIDIDriverAppleMIDIGetControlSocket( struct MIDIDriverAppleMIDI * driver, int * socket ) {
  *socket = driver->control_socket;
  return 0;
}

/**
 * @brief Handle incoming MIDI messages.
 * This is called by the RTP-MIDI payload parser whenever it encounters a new MIDI message.
 * There may be multiple messages in a single packet so a single call of @c MIDIDriverAppleMIDI
 * may trigger multiple calls of this function.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param message The message that was just received.
 * @retval 0 on success.
 * @retval >0 if the message could not be processed.
 */
int MIDIDriverAppleMIDIReceiveMessage( struct MIDIDriverAppleMIDI * driver, struct MIDIMessage * message ) {
  return MIDIMessageQueuePush( driver->in_queue, message );
}

/**
 * @brief Process outgoing MIDI messages.
 * This is called by the generic driver interface to pass messages to this driver implementation.
 * The driver may queue outgoing messages to reduce package overhead, trading of latency for throughput.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param message The message that should be sent.
 * @retval 0 on success.
 * @retval >0 if the message could not be processed.
 */
int MIDIDriverAppleMIDISendMessage( struct MIDIDriverAppleMIDI * driver, struct MIDIMessage * message ) {
  return MIDIMessageQueuePush( driver->out_queue, message );
}


static int _applemidi_init_addr_with_peer( struct AppleMIDICommand * command, struct RTPPeer * peer ) {
  struct sockaddr * addr;
  socklen_t size;

  RTPPeerGetAddress( peer, &size, &addr );
  memcpy( &(command->addr), &addr, size );
  command->size = size;
  return 0;
}

/**
 * @brief Test incoming packets for the AppleMIDI signature.
 * Check if the data that is waiting on a socket begins with the special AppleMIDI signature (0xffff).
 * @private @memberof MIDIDriverAppleMIDI
 * @param socket The socket.
 * @retval 0 if the packet is AppleMIDI
 * @retval 1 if the packet is not AppleMIDI
 * @retval -1 if no signature data could be received
 */
static int _test_applemidi( int socket ) {
  ssize_t bytes;
  short buf[2];
  bytes = recv( socket, &buf, 2, MSG_PEEK );
  if( bytes != 4 ) return -1;
  if( ntohs(buf[0]) == APPLEMIDI_PROTOCOL_SIGNATURE ) {
    switch( ntohs(buf[1]) ) {
      case APPLEMIDI_COMMAND_INVITATION:
      case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
      case APPLEMIDI_COMMAND_INVITATION_REJECTED:
      case APPLEMIDI_COMMAND_RECEIVER_FEEDBACK:
      case APPLEMIDI_COMMAND_SYNCHRONIZATION:
      case APPLEMIDI_COMMAND_ENDSESSION:
        return 0;
    }
  }
  return 1;
}

/**
 * @brief Send the given AppleMIDI command.
 * Compose a message buffer and send the datagram to the given peer.
 * @private @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param command The command.
 * @retval 0 On success.
 * @retval >0 If the packet could not be sent.
 */
static int _applemidi_send_command( struct MIDIDriverAppleMIDI * driver, int fd, struct AppleMIDICommand * command ) {
  unsigned long ssrc;
  unsigned long msg[8];
  int len;
  
  msg[0] = htonl( ( APPLEMIDI_PROTOCOL_SIGNATURE << 16 ) | command->type );
  switch( command->type ) {
      case APPLEMIDI_COMMAND_INVITATION:
      case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
      case APPLEMIDI_COMMAND_INVITATION_REJECTED:
      case APPLEMIDI_COMMAND_ENDSESSION:
        ssrc   = command->data.session.ssrc;
        msg[1] = htonl( command->data.session.version );
        msg[2] = htonl( command->data.session.token );
        msg[3] = htonl( command->data.session.ssrc );
        if( command->data.session.name[0] != '\0' ) {
          len = strlen( command->data.session.name );
          memcpy( &(msg[4]), command->data.session.name, len );
          len += sizeof( unsigned long ) * 4;
        } else {
          len  = sizeof( unsigned long ) * 4;
        }
        break;
      case APPLEMIDI_COMMAND_SYNCHRONIZATION:
        ssrc   = command->data.sync.ssrc;
        msg[1] = htonl( command->data.sync.ssrc );
        msg[2] = htonl( command->data.sync.count );
        msg[3] = htonl( command->data.sync.timestamp1 );
        msg[4] = htonl( command->data.sync.timestamp2 );
        msg[5] = htonl( command->data.sync.timestamp3 );
        len    = sizeof( unsigned long ) * 6;
        break;
      case APPLEMIDI_COMMAND_RECEIVER_FEEDBACK:
        ssrc   = command->data.feedback.ssrc;
        msg[1] = htonl( command->data.feedback.ssrc );
        msg[2] = htonl( command->data.feedback.seqnum );
        break;
      default:
        return 1;
  }

  sendto( fd, &msg[0], len, 0,
          (struct sockaddr *) &(command->addr), command->size );

  return 0;
}

/**
 * @brief Receive an AppleMIDI command.
 * Receive a datagram and decompose the message into the message structure.
 * @private @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param command The command.
 * @retval 0 On success.
 * @retval >0 If the packet could not be sent.
 */
static int _applemidi_recv_command( struct MIDIDriverAppleMIDI * driver, int fd, struct AppleMIDICommand * command ) {
  unsigned long ssrc;
  unsigned long msg[8];
  int len;
  
  len = recvfrom( fd, &msg[0], sizeof(msg), 0,
                  (struct sockaddr *) &(command->addr), &(command->size) );

  command->type = ntohl( msg[0] ) & 0xffff;
  
  switch( command->type ) {
      case APPLEMIDI_COMMAND_INVITATION:
      case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
      case APPLEMIDI_COMMAND_INVITATION_REJECTED:
      case APPLEMIDI_COMMAND_ENDSESSION:
        command->data.session.version = ntohl( msg[1] );
        command->data.session.token   = ntohl( msg[2] );
        command->data.session.ssrc    = ntohl( msg[3] );
        len -= 4 * sizeof(unsigned long);
        if( len > 0 ) {
          if( len > sizeof( command->data.session.name ) - 1 ) {
            len = sizeof( command->data.session.name ) - 1;
          }
          memcpy( &(command->data.session.name[0]), &msg[4], len ); 
          command->data.session.name[len] = '\0';
        }
        ssrc = command->data.session.ssrc;
        break;
      case APPLEMIDI_COMMAND_SYNCHRONIZATION:
        command->data.sync.ssrc       = ntohl( msg[1] );
        command->data.sync.count      = ntohl( msg[2] );
        command->data.sync.timestamp1 = ntohl( msg[3] );
        command->data.sync.timestamp2 = ntohl( msg[4] );
        command->data.sync.timestamp3 = ntohl( msg[5] );
        ssrc = command->data.sync.ssrc;
        break;
      case APPLEMIDI_COMMAND_RECEIVER_FEEDBACK:
        command->data.feedback.ssrc   = ntohl( msg[1] );
        command->data.feedback.seqnum = ntohl( msg[2] );
        ssrc = command->data.feedback.ssrc;
        break;
      default:
        return 1;
  }
  return 0;
}

/**
 * @brief Start or continue a synchronization session.
 * Continue a synchronization session identified by a given command.
 * The command must contain a pointer to a valid peer.
 * @param driver The driver.
 * @param fd The file descriptor to be used for communication.
 * @param command The previous sync command.
 * @retval 0 On success.
 * @retval >0 If the synchronization failed.
 */
static int _applemidi_sync( struct MIDIDriverAppleMIDI * driver, int fd, struct AppleMIDICommand * command ) {
  struct RTPPeer * peer = NULL;
  unsigned long ssrc, timestamp, diff;
  RTPSessionGetSSRC( driver->rtp_session, &ssrc );
  RTPSessionGetTimestamp( driver->rtp_session, &timestamp );

  if( command->type != APPLEMIDI_COMMAND_SYNCHRONIZATION || 
      command->data.sync.ssrc == ssrc ) {
    command->type = APPLEMIDI_COMMAND_SYNCHRONIZATION;
    command->data.sync.ssrc       = ssrc;
    command->data.sync.count      = 1;
    command->data.sync.timestamp1 = timestamp;
    return _applemidi_send_command( driver, fd, command );
  } else {
    RTPSessionFindPeerBySSRC( driver->rtp_session, &peer, command->data.sync.ssrc );

    /* received packet from other peer */
    if( command->data.sync.count == 3 ) {
      /* compute media delay */
      diff = ( command->data.sync.timestamp3 - command->data.sync.timestamp1 ) / 2;
      /* approximate time difference between peer and self */
      diff = command->data.sync.timestamp3 + diff - timestamp;

      /* RTPPeerSetTimestampDiff( command->peer, diff ) */
      /* finished sync */
      command->data.sync.ssrc       = ssrc;
      command->data.sync.count      = 0;
      return 0;
    }
    if( command->data.sync.count == 2 ) {
      /* compute media delay */
      diff = ( command->data.sync.timestamp3 - command->data.sync.timestamp1 ) / 2;
      /* approximate time difference between peer and self */
      diff = command->data.sync.timestamp2 + diff - timestamp;

      /* RTPPeerSetTimestampDiff( command->peer, diff ) */

      command->data.sync.ssrc       = ssrc;
      command->data.sync.count      = 3;
      command->data.sync.timestamp3 = timestamp;
      return _applemidi_send_command( driver, fd, command );
    }
    if( command->data.sync.count == 1 ) {
      command->data.sync.ssrc       = ssrc;
      command->data.sync.count      = 2;
      command->data.sync.timestamp2 = timestamp;
      return _applemidi_send_command( driver, fd, command );
    }
  }
  return 1;
}

static int _applemidi_start_sync( struct MIDIDriverAppleMIDI * driver, int fd, struct RTPPeer * peer ) {
  struct AppleMIDICommand command;
  _applemidi_init_addr_with_peer( &command, peer );
  return _applemidi_sync( driver, fd, &command );
}

/**
 * @brief Respond to a given AppleMIDI command.
 * Use the command as response and - if neccessary - send it back to the peer.
 * @private @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param command The command.
 * @retval 0 On success.
 * @retval >0 If the packet could not be sent.
 */
static int _applemidi_respond( struct MIDIDriverAppleMIDI * driver, int fd, struct AppleMIDICommand * command ) {
  struct RTPPeer * peer;

  switch( command->type ) {
    case APPLEMIDI_COMMAND_INVITATION:
      if( 1 ) {
        command->type = APPLEMIDI_COMMAND_INVITATION_ACCEPTED;
      } else {
        command->type = APPLEMIDI_COMMAND_INVITATION_REJECTED;
      }
      RTPSessionGetSSRC( driver->rtp_session, &(command->data.session.ssrc) );
      return _applemidi_send_command( driver, fd, command );
    case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
      peer = RTPPeerCreate( command->data.session.ssrc, command->size, (struct sockaddr *) &(command->addr) );
      RTPSessionAddPeer( driver->rtp_session, peer );
      RTPPeerRelease( peer );
      break;
    case APPLEMIDI_COMMAND_INVITATION_REJECTED:
      break;
    case APPLEMIDI_COMMAND_ENDSESSION:
      RTPSessionFindPeerBySSRC( driver->rtp_session, &peer, command->data.session.ssrc );
      RTPSessionRemovePeer( driver->rtp_session, peer );
      break;
    case APPLEMIDI_COMMAND_SYNCHRONIZATION:
      return _applemidi_sync( driver, fd, command );
    case APPLEMIDI_COMMAND_RECEIVER_FEEDBACK:
      RTPSessionFindPeerBySSRC( driver->rtp_session, &peer, command->data.feedback.ssrc );
      RTPMIDISessionTrunkateSendJournal( driver->rtpmidi_session, peer, command->data.feedback.seqnum );
      break;
  }
  return 0;
}

static int _applemidi_invite( struct MIDIDriverAppleMIDI * driver, socklen_t size, struct sockaddr * addr ) {
  struct AppleMIDICommand command;

  memcpy( &(command.addr), addr, size );
  command.size = size;
  command.type = APPLEMIDI_COMMAND_INVITATION;
  command.data.session.version = 1;
  command.data.session.token   = driver->token;
  RTPSessionGetSSRC( driver->rtp_session, &(command.data.session.ssrc) );
  strcpy( &(command.data.session.name[0]), "MIDIKit" );

  return _applemidi_send_command( driver, driver->control_socket, &command );
}

/**
 * @brief Connect to a peer.
 * Use the AppleMIDI protocol to establish an RTP-session, including a SSRC that was received
 * from the peer.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param address The internet address of the peer.
 * @param port The AppleMIDI control port (usually 5004), the RTP-port is the next port.
 * @retval 0 on success.
 * @retval >0 if the connection could not be established.
 */
int MIDIDriverAppleMIDIAddPeer( struct MIDIDriverAppleMIDI * driver, char * address, unsigned short port ) {
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_port   = htons( port );
  inet_aton( address, &(addr.sin_addr) );

  return _applemidi_invite( driver, sizeof(addr), (struct sockaddr *) &addr );
}

/**
 * @brief Disconnect from a peer.
 * Use the AppleMIDI protocol to tell the peer that the session ended.
 * Remove the peer from the @c RTPSession.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param address The internet address of the peer.
 * @param port The AppleMIDI control port (usually 5004), the RTP-port is the next port.
 * @retval 0 on success.
 * @retval >0 if the session could not be ended.
 */
int MIDIDriverAppleMIDIRemovePeer( struct MIDIDriverAppleMIDI * driver, char * address, unsigned short port ) {
  struct RTPPeer * peer;
  struct sockaddr_in addr;
  int result;

  addr.sin_family = AF_INET;
  addr.sin_port   = htons( port );
  inet_aton( address, &(addr.sin_addr) );

  result = RTPSessionFindPeerByAddress( driver->rtp_session, &peer, sizeof(addr), (struct sockaddr *) &addr );
  if( result ) return result;

  /* send endsession "BY" command */
  
  return RTPSessionRemovePeer( driver->rtp_session, peer );
}

/**
 * @brief Receive from any peer.
 * This should be called whenever there is new data to be received on a socket.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @retval 0 on success.
 * @retval >0 if the packet could not be processed.
 */
int MIDIDriverAppleMIDIReceive( struct MIDIDriverAppleMIDI * driver ) {
  int fd;
  fd_set fds;
  static struct timeval tv = { 0, 0 };
  struct AppleMIDICommand command;
  
  /* check for available data on sockets (select()) */
  FD_ZERO(&fds);
  FD_SET(driver->control_socket, &fds);
  FD_SET(driver->rtp_socket, &fds);

  select( 2, &fds, NULL, NULL, &tv );
  
  if( FD_ISSET( driver->control_socket, &fds ) ) {
    fd = driver->control_socket;
  } else if( FD_ISSET( driver->rtp_socket, &fds ) ) {
    fd = driver->rtp_socket;
  } else {
    return 0;
  }
  
  if( _test_applemidi( fd ) ) {
    if( _applemidi_recv_command( driver, fd, &command ) == 0 ) {
      return _applemidi_respond( driver, fd, &command );
    }
  } else if( fd == driver->rtp_socket ) {
    RTPMIDISessionReceive( driver->rtpmidi_session, 0, NULL, NULL, NULL );
  }
  
  return 0;
}

/**
 * @brief Send queued messages to all connected peers.
 * This should be called whenever new messages are added to the queue and whenever the
 * socket can accept new data.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @retval 0 on success.
 * @retval >0 if packets could not be sent, or any other operation failed.
 */
int MIDIDriverAppleMIDISend( struct MIDIDriverAppleMIDI * driver ) {
  int fd, i;
  size_t length;
  fd_set fds;
  static struct timeval tv = { 0, 0 };
  struct MIDIMessage * msg_list[8];

  MIDIMessageQueueGetLength( driver->out_queue, &length );
  
  if( length == 0 ) return 0;
  
  /* check if sockets can accept data (select()) */
  FD_ZERO(&fds);
  FD_SET(driver->control_socket, &fds);
  FD_SET(driver->rtp_socket, &fds);

  select( 2, NULL, &fds, NULL, &tv );
  
  if( FD_ISSET( driver->rtp_socket, &fds ) ) {
    fd = driver->rtp_socket;
  } else if( FD_ISSET( driver->control_socket, &fds ) ) {
    fd = driver->control_socket;
  } else {
    return 0;
  }
  
  for( i=0; i<8 && i<length; i++ ) {
    MIDIMessageQueuePop( driver->out_queue, &msg_list[i] );
  }
  
  RTPMIDISessionSend( driver->rtpmidi_session, i, &msg_list[0], &length, NULL );
  while( length < i ) {
    i -= length;
    RTPMIDISessionSend( driver->rtpmidi_session, i, &msg_list[length], &length, NULL );
  }
  return 0;
}

/**
 * @brief Do idling operations.
 * When there is nothing else to do, keep in sync with connected peers,
 * dispatch incoming messages, send receiver feedback.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @retval 0 on success.
 * @retval >0 if packets could not be sent, or any other operation failed.
 */
int MIDIDriverAppleMIDIIdle( struct MIDIDriverAppleMIDI * driver ) {
  size_t length;
  MIDIMessageQueueGetLength( driver->in_queue, &length );

  /* check for messages in dispatch (incoming) queue:
   *   if message needs to be dispatched (timestamp >= now+latency)
   *   call MIDIDriverAppleMIDIReceiveMessage
   * send receiver feedback
   * if the last synchronization happened a certain time ago, synchronize again */
  return 1;
}