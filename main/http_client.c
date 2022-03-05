#include "http_client.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

static const char *TAG = "HTTP_CLIENT  ";

/*
	Root cert for howsmyssl.com, taken from howsmyssl_com_root_cert.pem

	The PEM file was extracted from the output of this command:
	openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

	The CA root cert is the last cert given in the chain of certs.

	To embed it in the app binary, the PEM file is named
	in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[] asm("_binary_howsmyssl_com_root_cert_pem_end");
extern const char postman_root_cert_pem_start[] asm("_binary_postman_root_cert_pem_start");
extern const char postman_root_cert_pem_end[] asm("_binary_postman_root_cert_pem_end");

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
	static char *output_buffer; //	Buffer to store response of http request from event handler
	static int output_len;		// 	Stores number of bytes read

	switch (evt->event_id)
	{
	case HTTP_EVENT_ERROR:
		ESP_LOGD(TAG, "HTTP_EVENT_ERROR");

		break;

	case HTTP_EVENT_ON_CONNECTED:
		ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");

		break;

	case HTTP_EVENT_HEADER_SENT:
		ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");

		break;

	case HTTP_EVENT_ON_HEADER:
		ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, Key = %s, Value = %s", evt->header_key, evt->header_value);

		break;

	case HTTP_EVENT_ON_DATA:
		ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, Length = %d", evt->data_len);
		/*
		 *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
		 *  However, event handler can also be used in case chunked encoding is used.
		 */
		if (!esp_http_client_is_chunked_response(evt->client))
		{
			// 	If user_data buffer is configured, copy the response into the buffer
			if (evt->user_data)
				memcpy(evt->user_data + output_len, evt->data, evt->data_len);
			else
			{
				if (output_buffer == NULL)
				{
					output_buffer = (char *)malloc(esp_http_client_get_content_length(evt->client));
					output_len = 0;
					if (output_buffer == NULL)
					{
						ESP_LOGE(TAG, "Failed to Allocate Memory for Output Buffer");

						return ESP_FAIL;
					}
				}

				memcpy(output_buffer + output_len, evt->data, evt->data_len);
			}

			output_len += evt->data_len;
		}

		break;

	case HTTP_EVENT_ON_FINISH:
		if (output_buffer != NULL)
		{
			// 	Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
			// 	ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
			free(output_buffer);
			output_buffer = NULL;
		}

		output_len = 0;

		break;

	case HTTP_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");

		int mbedtls_err = 0;
		esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);

		if (err != 0)
		{
			if (output_buffer != NULL)
			{
				free(output_buffer);
				output_buffer = NULL;
			}

			output_len = 0;
			ESP_LOGI(TAG, "Last ESP Error Code  : 0x%x", err);
			ESP_LOGI(TAG, "Last MBEDtls Failure : 0x%x", mbedtls_err);
		}

		break;
	}

	return ESP_OK;
}
void http_put_version(char *mac_put, const char *vertion)
{
	char url[200];

	sprintf(url, "%s/api/device-versions/%s/version", WEB_SERVER, mac_put);

	esp_http_client_config_t config =
		{
			//.host = WEB_SERVER,
			//.path = "/devices/{deviceCode}/{}",
			.url = url,
			.event_handler = _http_event_handler,
			.auth_type = HTTP_AUTH_TYPE_NONE,
		};

	esp_http_client_handle_t client = esp_http_client_init(&config);

	// 	PUT
	char post_data[200];

	sprintf(post_data, "{\"version\": \"%s\"}", vertion);

	esp_http_client_set_method(client, HTTP_METHOD_PUT);
	esp_http_client_set_header(client, "Content-Type", "application/json");
	esp_http_client_set_post_field(client, post_data, strlen(post_data));

	esp_err_t err = esp_http_client_perform(client);

	if (err == ESP_OK)
	{
		ESP_LOGI(TAG, "HTTP PUT Status = %d, content_length = %d",
				 esp_http_client_get_status_code(client),
				 esp_http_client_get_content_length(client));
	}
	else
		ESP_LOGE(TAG, "HTTP POST Request Failed : %s", esp_err_to_name(err));

	esp_http_client_cleanup(client);
}

char *http_get_version(char *mac_address)
{
	char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
	char url[200];

	sprintf(url, "%s/api/device-versions/%s/version", WEB_SERVER, mac_address);

	ESP_LOGI(TAG, "Check URL Version : %s", url);

	esp_http_client_config_t config =
		{
			.url = url,
			//.host 		= 	WEB_SERVER						,
			//.path 		= 	path_str						,
			.event_handler = _http_event_handler,
			.auth_type = HTTP_AUTH_TYPE_NONE,
			.user_data = local_response_buffer,
			//.cert_pem 	= 	( char *)server_cert_pem_start	,
		};

	esp_http_client_handle_t client = esp_http_client_init(&config);

	// 	GET
	esp_err_t err = esp_http_client_perform(client);

	if (err == ESP_OK)
	{
		ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
				 esp_http_client_get_status_code(client),
				 esp_http_client_get_content_length(client));
	}
	else
		ESP_LOGE(TAG, "HTTP GET Request Failed : %s", esp_err_to_name(err));

	// ESP_LOG_BUFFER_HEX	( TAG, local_response_buffer, strlen ( local_response_buffer ) )							;
	esp_http_client_cleanup(client);

	// char tesst[] 		= 	"{\"logicCode\":1,\"message\":\"\",\"data\":{\"id\":27301,\"version\":\"1.1.2\",\"urlUpgrade\":\"https://storage.googleapis.com/pettracker-staging.appspot.com/device-versions/1.1.2.bin\",\"createdBy\":1,\"createdAt\":\"2021-08-23T02:41:57Z\",\"currentVersion\":\"1.0.0\",\"agreeUpgrade\":false,\"upgradeAt\":\"2021-08-23T07:52:31Z\",\"upgradeBy\":1952,\"isDifferent\":true}}";
	// strcpy			( local_response_buffer, tesst )																;

	char *ptr = local_response_buffer;

	return ptr;
}