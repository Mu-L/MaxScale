/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2024-07-07
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

/**
 * @file dcb.h  The Descriptor Control Block
 */

#include <maxscale/ccdefs.hh>

#include <openssl/ssl.h>
#include <netinet/in.h>

#include <maxbase/poll.h>
#include <maxscale/authenticator.hh>
#include <maxscale/buffer.hh>
#include <maxscale/modinfo.h>
#include <maxscale/protocol.hh>

#include <memory>

class SERVICE;

MXS_BEGIN_DECLS

#define ERRHANDLE

struct MXS_SESSION;
struct SERVER;

struct DCB;

#define DCBFD_CLOSED -1

/**
 * The statistics gathered on a descriptor control block
 */
typedef struct dcbstats
{
    int n_reads;        /*< Number of reads on this descriptor */
    int n_writes;       /*< Number of writes on this descriptor */
    int n_accepts;      /*< Number of accepts on this descriptor */
    int n_buffered;     /*< Number of buffered writes */
    int n_high_water;   /*< Number of crosses of high water mark */
    int n_low_water;    /*< Number of crosses of low water mark */
} DCBSTATS;

#define DCBSTATS_INIT {0}

/* DCB states */
typedef enum
{
    DCB_STATE_UNDEFINED,    /*< State variable with no state */
    DCB_STATE_ALLOC,        /*< Memory allocated but not populated */
    DCB_STATE_POLLING,      /*< Waiting in the poll loop */
    DCB_STATE_LISTENING,    /*< The DCB is for a listening socket */
    DCB_STATE_DISCONNECTED, /*< The socket is now closed */
    DCB_STATE_NOPOLLING,    /*< Removed from poll mask */
} dcb_state_t;

#define STRDCBSTATE(s) \
    ((s) == DCB_STATE_ALLOC ? "DCB_STATE_ALLOC"      \
                            : ((s) == DCB_STATE_POLLING ? "DCB_STATE_POLLING"   \
                                                        : ((s) == DCB_STATE_LISTENING ? "DCB_STATE_LISTENING"   \
                                                                                      : ((s) \
                                                                                         == \
                                                                                         DCB_STATE_DISCONNECTED \
                                                                                         ? \
                                                                                         "DCB_STATE_DISCONNECTED"   \
                                                                                         : (( \
                                                                                                s) \
                                                                                            == \
                                                                                            DCB_STATE_NOPOLLING \
                                                                                            ? \
                                                                                            "DCB_STATE_NOPOLLING"   \
                                                                                            : (( \
                                                                                                   s) \
                                                                                               == \
                                                                                               DCB_STATE_UNDEFINED \
                                                                                               ? \
                                                                                               "DCB_STATE_UNDEFINED" \
                                                                                               : \
                                                                                               "DCB_STATE_UNKNOWN"))))))

/**
 * Callback reasons for the DCB callback mechanism.
 */
typedef enum
{
    DCB_REASON_CLOSE,               /*< The DCB is closing */
    DCB_REASON_DRAINED,             /*< The write delay queue has drained */
    DCB_REASON_HIGH_WATER,          /*< Cross high water mark */
    DCB_REASON_LOW_WATER,           /*< Cross low water mark */
    DCB_REASON_ERROR,               /*< An error was flagged on the connection */
    DCB_REASON_HUP,                 /*< A hangup was detected */
    DCB_REASON_NOT_RESPONDING       /*< Server connection was lost */
} DCB_REASON;

/**
 * Callback structure - used to track callbacks registered on a DCB
 */
typedef struct dcb_callback
{
    DCB_REASON reason;          /*< The reason for the callback */
    int (* cb)(DCB* dcb, DCB_REASON reason, void* userdata);
    void*                userdata;      /*< User data to be sent in the callback */
    struct dcb_callback* next;          /*< Next callback for this DCB */
} DCB_CALLBACK;

/**
 * State of SSL connection
 */
typedef enum
{
    SSL_HANDSHAKE_UNKNOWN,          /*< The DCB has unknown SSL status */
    SSL_HANDSHAKE_REQUIRED,         /*< SSL handshake is needed */
    SSL_HANDSHAKE_DONE,             /*< The SSL handshake completed OK */
    SSL_ESTABLISHED,                /*< The SSL connection is in use */
    SSL_HANDSHAKE_FAILED            /*< The SSL handshake failed */
} SSL_STATE;

/**
 * Descriptor Control Block
 *
 * A wrapper for a network descriptor within the gateway, it contains all the
 * state information necessary to allow for the implementation of the asynchronous
 * operation of the protocol and gateway functions. It also provides links to the service
 * and session data that is required to route the information within the gateway.
 *
 * It is important to hold the state information here such that any thread within the
 * gateway may be selected to execute the required actions when a network event occurs.
 *
 * Note that the first few fields (up to and including "entry_is_ready") must
 * precisely match the LIST_ENTRY structure defined in the list manager.
 */
struct DCB : public MXB_POLL_DATA
{
    enum class Role
    {
        CLIENT,         /*< Serves dedicated client */
        BACKEND,        /*< Serves back end connection */
        INTERNAL        /*< Internal DCB not connected to the outside */
    };

    DCB(Role role, MXS_SESSION*);
    ~DCB();

    /**
     * DCB type in string form
     */
    const char* type();

    bool                    dcb_errhandle_called = false;   /**< this can be called only once */
    Role                    role;
    int                     fd = DCBFD_CLOSED;                  /**< The descriptor */
    dcb_state_t             state = DCB_STATE_ALLOC;            /**< Current descriptor state */
    SSL_STATE               ssl_state = SSL_HANDSHAKE_UNKNOWN;  /**< Current state of SSL if in use */
    char*                   remote = nullptr;                   /**< Address of remote end */
    char*                   user = nullptr;                     /**< User name for connection */
    struct sockaddr_storage ip;                                 /**< remote IPv4/IPv6 address */
    void*                   protocol = nullptr;                 /**< The protocol specific state */
    size_t                  protocol_packet_length = 0;         /**< protocol packet length */
    size_t                  protocol_bytes_processed = 0;       /**< How many bytes have been read */
    MXS_SESSION*            session;                            /**< The owning session */
    MXS_PROTOCOL            func = {};                          /**< Protocol functions for the DCB */
    MXS_AUTHENTICATOR       authfunc = {};                      /**< Authenticator functions for the DCB */
    uint64_t                writeqlen = 0;                      /**< Bytes in writeq */
    uint64_t                high_water = 0;                     /**< High water mark of write queue */
    uint64_t                low_water = 0;                      /**< Low water mark of write queue */
    GWBUF*                  writeq = nullptr;                   /**< Write Data Queue */
    GWBUF*                  delayq = nullptr;                   /**< Delay Backend Write Data Queue */
    GWBUF*                  readq = nullptr;                    /**< Read queue for incomplete reads */
    GWBUF*                  fakeq = nullptr;                    /**< Fake event queue for generated events */
    uint32_t                fake_event = 0;                     /**< Fake event to be delivered to handler */

    DCBSTATS stats = {};                        /**< DCB related statistics */
    DCB*     nextpersistent = nullptr;          /**< Next DCB in the persistent pool for SERVER */
    time_t   persistentstart = 0;               /**<    0: Not in the persistent pool.
                                                 *      -1: Evicted from the persistent pool and being closed.
                                                 *   non-0: Time when placed in the persistent pool.
                                                 */
    bool           valid_for_pool = true;       // TODO: Make this private in 2.5
    SERVICE*       service = nullptr;           /**< The related service */
    void*          data = nullptr;              /**< Client protocol data, owned by client DCB */
    void*          authenticator_data = nullptr;/**< The authenticator data for this DCB */
    DCB_CALLBACK*  callbacks = nullptr;         /**< The list of callbacks for the DCB */
    int64_t        last_read = 0;               /**< Last time the DCB received data */
    int64_t        last_write = 0;              /**< Last time the DCB sent data */
    struct SERVER* server = nullptr;            /**< The associated backend server */
    SSL*           ssl = nullptr;               /**< SSL struct for connection */
    bool           ssl_read_want_read = false;
    bool           ssl_read_want_write = false;
    bool           ssl_write_want_read = false;
    bool           ssl_write_want_write = false;
    bool           was_persistent = false;      /**< Whether this DCB was in the persistent pool */
    bool           high_water_reached = false;  /** High water mark reached, to determine whether we need to
                                                 * release
                                                 * throttle */

    // Enable this to prevent write errors from being logged. Helps reduce false errors when writes to
    // potentially closed DCBs are done.
    bool silence_write_errors = false;

    struct
    {
        DCB* next = nullptr;        /**< Next DCB in owning thread's list */
        DCB* tail = nullptr;        /**< Last DCB in owning thread's list */
    }        thread;
    uint32_t n_close = 0;   /** How many times dcb_close has been called. */
    uint64_t m_uid;         /**< Unique identifier for this DCB */
    bool     is_fake_event = false;
};

/**
 * The DCB usage filer used for returning DCB's in use for a certain reason
 */
typedef enum
{
    DCB_USAGE_CLIENT,
    DCB_USAGE_LISTENER,
    DCB_USAGE_BACKEND,
    DCB_USAGE_INTERNAL,
    DCB_USAGE_ALL
} DCB_USAGE;

/* A few useful macros */
#define DCB_SESSION(x)            (x)->session
#define DCB_PROTOCOL(x, type)     (type*)((x)->protocol)
#define DCB_WRITEQLEN(x)          (x)->writeqlen
#define DCB_SET_LOW_WATER(x, lo)  (x)->low_water = (lo);
#define DCB_SET_HIGH_WATER(x, hi) (x)->low_water = (hi);
#define DCB_BELOW_LOW_WATER(x)    ((x)->low_water && (x)->writeqlen < (x)->low_water)
#define DCB_ABOVE_HIGH_WATER(x)   ((x)->high_water && (x)->writeqlen > (x)->high_water)
#define DCB_THROTTLING_ENABLED(x) ((x)->high_water && (x)->low_water)
/**
 * @brief DCB system initialization function
 *
 * This function needs to be the first function call into this system.
 */
void dcb_global_init();

int  dcb_write(DCB*, GWBUF*);
DCB* dcb_alloc(DCB::Role, MXS_SESSION*);
DCB* dcb_connect(struct SERVER*, MXS_SESSION*, const char*);
int  dcb_read(DCB*, GWBUF**, int);
int  dcb_bytes_readable(DCB* dcb);
int  dcb_drain_writeq(DCB*);
void dcb_close(DCB*);

/**
 * @brief Close DCB in the thread that owns it.
 *
 * @param dcb The dcb to be closed.
 *
 * @note Even if the calling thread owns the dcb, the closing will
 *       still be made via the event loop.
 */
void dcb_close_in_owning_thread(DCB* dcb);

void printAllDCBs();                                                            /* Debug to print all DCB in
                                                                                 * the system */
void printDCB(DCB*);                                                            /* Debug print routine */
void dprintDCBList(DCB*);                                                       /* Debug print DCB list
                                                                                 * statistics */
void dprintAllDCBs(DCB*);                                                       /* Debug to print all DCB in
                                                                                 * the system */
void dprintOneDCB(DCB*, DCB*);                                                  /* Debug to print one DCB */
void dprintDCB(DCB*, DCB*);                                                     /* Debug to print a DCB in the
                                                                                 * system */
void dListDCBs(DCB*);                                                           /* List all DCBs in the system
                                                                                 * */
void dListClients(DCB*);                                                        /* List al the client DCBs */
const char* gw_dcb_state2string(dcb_state_t);                                   /* DCB state to string */
void dcb_printf(DCB*, const char*, ...) __attribute__ ((format(printf, 2, 3))); /* DCB version of printf */
int dcb_add_callback(DCB*, DCB_REASON, int (*)(DCB*, DCB_REASON, void*), void*);
int dcb_remove_callback(DCB*, DCB_REASON, int (*)(DCB*, DCB_REASON, void*), void*);
int dcb_count_by_usage(DCB_USAGE);                      /* Return counts of DCBs */
int      dcb_persistent_clean_count(DCB*, int, bool);   /* Clean persistent and return count */
void     dcb_hangup_foreach(struct SERVER* server);
uint64_t dcb_get_session_id(DCB* dcb);
char*    dcb_role_name(DCB*);               /* Return the name of a role */
int      dcb_accept_SSL(DCB* dcb);
int      dcb_connect_SSL(DCB* dcb);
int      dcb_listen(DCB* listener, const char* config);
void     dcb_enable_session_timeouts();
void     dcb_process_timeouts(int thr);

/**
 * @brief Append a buffer the DCB's readqueue
 *
 * Usually data is stored into the DCB's readqueue when not enough data is
 * available and the processing needs to be deferred until more data is available.
 *
 * @param dcb    The DCB to be appended to.
 * @param buffer The buffer to append.
 */
static inline void dcb_readq_append(DCB* dcb, GWBUF* buffer)
{
    dcb->readq = gwbuf_append(dcb->readq, buffer);
}

/**
 * @brief Returns the read queue of the DCB.
 *
 * @note The read queue remains the property of the DCB.
 *
 * @return A buffer of NULL if there is no read queue.
 */
static GWBUF* dcb_readq_get(DCB* dcb)
{
    return dcb->readq;
}

/**
 * @brief Returns whether a DCB currently has a read queue.
 *
 * @return True, if the DCB has a read queue, otherwise false.
 */
static inline bool dcb_readq_has(DCB* dcb)
{
    return dcb->readq != NULL;
}

/**
 * @brief Returns the current length of the read queue
 *
 * @return Length of read queue
 */
static unsigned int dcb_readq_length(DCB* dcb)
{
    return dcb->readq ? gwbuf_length(dcb->readq) : 0;
}

/**
 * @brief Prepend a buffer the DCB's readqueue
 *
 * @param dcb    The DCB to be prepended to.
 * @param buffer The buffer to prepend
 */
static inline void dcb_readq_prepend(DCB* dcb, GWBUF* buffer)
{
    dcb->readq = dcb->readq ? gwbuf_append(buffer, dcb->readq) : buffer;
}

/**
 * @brief Returns the read queue of the DCB and sets the read queue to NULL.
 *
 * @note The read queue becomes the property of the caller.
 *
 * @return A buffer of NULL if there is no read queue.
 */
static GWBUF* dcb_readq_release(DCB* dcb)
{
    GWBUF* readq = dcb->readq;
    dcb->readq = NULL;
    return readq;
}

/**
 * @brief Set read queue of a DCB
 *
 * The expectation is that there is no readqueue when this is done.
 * The ownership of the provided buffer moved to the DCB.
 *
 * @param dcb    The DCB to be reset.
 * @param buffer The buffer to reset with
 */
static inline void dcb_readq_set(DCB* dcb, GWBUF* buffer)
{
    if (dcb->readq)
    {
        MXS_ERROR("Read-queue set when there already is a read-queue.");
        // TODO: Conceptually this should be freed here. However, currently
        // TODO: the code just assigns without checking, so we do the same
        // TODO: for now. If this is not set to NULL when it has been consumed,
        // TODO: we would get a double free.
        // TODO: gwbuf_free(dcb->readq);
        dcb->readq = NULL;
    }
    dcb->readq = buffer;
}

/**
 * @brief Call a function for each connected DCB
 *
 * @deprecated You should not use this function, use dcb_foreach_parallel instead
 *
 * @warning This must only be called from the main thread, otherwise deadlocks occur
 *
 * @param func Function to call. The function should return @c true to continue iteration
 * and @c false to stop iteration earlier. The first parameter is a DCB and the second
 * is the value of @c data that the user provided.
 * @param data User provided data passed as the second parameter to @c func
 * @return True if all DCBs were iterated, false if the callback returned false
 */
bool dcb_foreach(bool (* func)(DCB* dcb, void* data), void* data);

/**
 * @brief Call a function for each connected DCB on the current worker
 *
 * @param func Function to call. The function should return @c true to continue
 *             iteration and @c false to stop iteration earlier. The first parameter
 *             is the current DCB.
 *
 * @param data User provided data passed as the second parameter to @c func
 */
void dcb_foreach_local(bool (* func)(DCB* dcb, void* data), void* data);

/**
 * @brief Return the port number this DCB is connected to
 *
 * @param dcb DCB to inspect
 * @return Port number the DCB is connected to or -1 if information is not available
 */
int dcb_get_port(const DCB* dcb);

/**
 * @brief Return the DCB currently being handled by the calling thread.
 *
 * @return A DCB, or NULL if the calling thread is not currently handling
 *         a DCB or if the calling thread is not a polling/worker thread.
 */
DCB* dcb_get_current();

/**
 * Get JSON representation of the DCB
 *
 * @param dcb DCB to convert to JSON
 *
 * @return The JSON representation
 */
json_t* dcb_to_json(DCB* dcb);

/**
 * DCB flags values
 */
#define DCBF_HUNG    0x0002     /*< Hangup has been dispatched */
#define DCBF_REPLIED 0x0004     /*< DCB was written to */

#define DCB_REPLIED(d) ((d)->flags & DCBF_REPLIED)

MXS_END_DECLS
