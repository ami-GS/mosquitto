#include "config.h"
#include </usr/local/msquic/include/msquic.h>
#include </usr/local/msquic/include/msquic_posix.h>
#include "quic/common.h"
#include "memory_mosq.h"

bool Connected = false;

//
// Helper function to load a client configuration.
//
BOOLEAN
client_load_configuration(
    BOOLEAN Unsecure,
	struct mosquitto* mosq
    )
{
    QUIC_SETTINGS Settings_ = {0};
    //
    // Configures the client's idle timeout.
    //
    Settings_.IdleTimeoutMs = 10000;
    Settings_.IsSet.IdleTimeoutMs = TRUE;

    //
    // Configures a default client configuration, optionally disabling
    // server certificate validation.
    //
	fprintf(stderr, "9-2-1-1\n");
    QUIC_CREDENTIAL_CONFIG CredConfig;
    memset(&CredConfig, 0, sizeof(CredConfig));
    CredConfig.Type = QUIC_CREDENTIAL_TYPE_NONE;
    CredConfig.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
    if (Unsecure) {
        CredConfig.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    }

    //
    // Allocate/initialize the configuration object, with the configured ALPN
    // and settings.
    //
	fprintf(stderr, "9-2-1-2\n");
	//HQUIC Registration;
    QUIC_STATUS Status = QUIC_STATUS_SUCCESS;
    if (QUIC_FAILED(Status = MsQuic->ConfigurationOpen(mosq->Registration, &Alpn, 1, &Settings_, sizeof(Settings_), NULL, &mosq->Configuration))) {
        fprintf(stderr, "ConfigurationOpen failed, 0x%x!\n", Status);
        return FALSE;
    }

    //
    // Loads the TLS credential part of the configuration. This is required even
    // on client side, to indicate if a certificate is required or not.
    //
	fprintf(stderr, "9-2-1-3\n");
    if (QUIC_FAILED(Status = MsQuic->ConfigurationLoadCredential(mosq->Configuration, &CredConfig))) {
        fprintf(stderr, "ConfigurationLoadCredential failed, 0x%x!\n", Status);
        return FALSE;
    }

    return TRUE;
}

//
// Runs the client side of the protocol.
//
int
quic_connect(const char *host, uint16_t port, struct mosquitto *mosq)
{
    //
    // Load the client configuration based on the "unsecure" command line option.
    // TODO: change to secure flag
	fprintf(stderr, "9-2-1\n");
    if (!client_load_configuration(1, mosq)) {
		fprintf(stderr, "failed to do ClientLoadConfiguration\n");
        return 1;
    }

    QUIC_STATUS Status;
    const char* ResumptionTicketString = NULL;
	struct libmsquic_mqtt* connection_context = mosquitto__calloc(1, sizeof(struct libmsquic_mqtt));;
	if(!connection_context){
		// TODO: stream cancelation?
		log__printf(NULL, MOSQ_LOG_WARNING, "CRITICAL: allocating stream_context failed");
		return;
	}
	connection_context->mosq = mosq;

    //
    // Allocate a new connection object.
    //
	fprintf(stderr, "9-2-2\n");
	//if (QUIC_FAILED(Status = MsQuic->ConnectionOpen(mosq->Registration, client_connection_callback, connection_context, &mosq->Connection))) {
    if (QUIC_FAILED(Status = MsQuic->ConnectionOpen(mosq->Registration, connection_callback, connection_context, &mosq->Connection))) {
        printf("ConnectionOpen failed, 0x%x!\n", Status);
        goto Error;
    }

    // if ((ResumptionTicketString = GetValue(argc, argv, "ticket")) != NULL) {
    //     //
    //     // If provided at the command line, set the resumption ticket that can
    //     // be used to resume a previous session.
    //     //
    //     uint8_t ResumptionTicket[1024];
    //     uint16_t TicketLength = (uint16_t)DecodeHexBuffer(ResumptionTicketString, sizeof(ResumptionTicket), ResumptionTicket);
    //     if (QUIC_FAILED(Status = MsQuic->SetParam(Connection, QUIC_PARAM_LEVEL_CONNECTION, QUIC_PARAM_CONN_RESUMPTION_TICKET, TicketLength, ResumptionTicket))) {
    //         printf("SetParam(QUIC_PARAM_CONN_RESUMPTION_TICKET) failed, 0x%x!\n", Status);
    //         goto Error;
    //     }
    // }

    //
    // Get the target / server name or IP from the command line.
    //
    // const char* Target = "127.0.0.1:8883";
    // if ((Target = GetValue(argc, argv, "target")) == NULL) {
    //     printf("Must specify '-target' argument!\n");
    //     Status = QUIC_STATUS_INVALID_PARAMETER;
    //     goto Error;
    // }

    printf("[conn][%p] Connecting...\n", mosq->Connection);

    //
    // Start the connection to the server.
    //
	fprintf(stderr, "9-2-3 connect to %s:%d\n", host, port);
    if (QUIC_FAILED(Status = MsQuic->ConnectionStart(mosq->Connection, mosq->Configuration, QUIC_ADDRESS_FAMILY_UNSPEC, host, port))) {
        fprintf(stderr, "ConnectionStart failed, 0x%x!\n", Status);
        goto Error;
    }
	while(!Connected) {
		sleep(1);
		//fprintf(stderr, "Connected=%d\n", Connected);
	}

    return 0;

Error:

    if (QUIC_FAILED(Status) && mosq->Connection != NULL) {
        MsQuic->ConnectionClose(mosq->Connection);
    }
    return 1;
}

