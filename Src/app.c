 /**
 ******************************************************************************
 * @file    app.c
 * @author  G-DC
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 G-DC
 * All rights reserved.
 *
 * This software is provided AS-IS.
 *
 ******************************************************************************
 */

#include "app.h"

#include <stdint.h>

#include "app_cam.h"
#include "app_config.h"
#include "app_postprocess.h"
#include "isp_api.h"
#include "ll_aton_runtime.h"
#include "cmw_camera.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32_lcd.h"
#include "stm32_lcd_ex.h"
#include "stm32n6xx_hal.h"
#include "tx_api.h"
#include "utils.h"
#include <string.h> // For memcpy
#include "nx_user.h"
#include "tx_api.h"
#include "nxd_mqtt_client.h"
#include "nxd_sntp_client.h"
#include "nxd_dns.h"
#include "nx_stm32_eth_driver.h"
#include "stm32n6xx.h"
#include "app_threadx.h"

#define USE_STATIC_ALLOCATION                    1

#define TX_APP_MEM_POOL_SIZE                     1024

#define NX_APP_MEM_POOL_SIZE                     50*1024

#if (USE_STATIC_ALLOCATION == 1)
/* USER CODE BEGIN TX_Pool_Buffer */
/* USER CODE END TX_Pool_Buffer */
#if defined ( __ICCARM__ )
#pragma data_alignment=4
#endif
__ALIGN_BEGIN static UCHAR tx_byte_pool_buffer[TX_APP_MEM_POOL_SIZE] __ALIGN_END;
static TX_BYTE_POOL tx_app_byte_pool;

/* USER CODE BEGIN NX_Pool_Buffer */
#if defined ( __ICCARM__ ) /* IAR Compiler */
#pragma location = ".NetXPoolSection"
#else /* GNU and AC6 compilers */
__attribute__((section(".NetXPoolSection")))
#endif
/* USER CODE END NX_Pool_Buffer */
#if defined ( __ICCARM__ )
#pragma data_alignment=4
#endif
__ALIGN_BEGIN static UCHAR nx_byte_pool_buffer[NX_APP_MEM_POOL_SIZE] __ALIGN_END;
static TX_BYTE_POOL nx_app_byte_pool;

#endif

TX_THREAD      NxAppThread;
NX_PACKET_POOL NxAppPool;
NX_IP          NetXDuoEthIpInstance;
TX_SEMAPHORE   DHCPSemaphore;
NX_DHCP        DHCPClient;

ULONG          IpAddress;
ULONG          NetMask;
/* USER CODE BEGIN PV */
//extern RNG_HandleTypeDef hrng;

TX_THREAD AppMQTTClientThread;
TX_THREAD AppMQTTClientThreadLog;
TX_THREAD AppSNTPThread;
TX_THREAD AppLinkThread;

TX_QUEUE  MsgQueueOne;

NXD_MQTT_CLIENT MqttClient;
NX_SNTP_CLIENT  SntpClient;
static NX_DNS   DnsClient;
uint8_t MACAddr[6];
RTC_HandleTypeDef hrtc;
uint8_t prev_nb_person;
ULONG prev_timestamp;
int prev_state_detect;
// Shared resources between threads
#define MEASUREMENT_QUEUE_DEPTH  20
#define MEASUREMENT_QUEUE_MSG_SIZE sizeof(sensor_data_t)

/* Define a structure for your sensor data */
typedef struct {
    float nb_detect;
    ULONG timestamp;
    int state_detect;
} sensor_data_t;
typedef struct NXD_MQTT_MESSAGE_STRUCT {
    UCHAR *topic;       /* Pointer to message topic */
    ULONG topic_length; /* Topic length in bytes */
    UCHAR *payload;     /* Pointer to message payload */
    ULONG payload_length; /* Payload length in bytes */
    UCHAR qos_level;    /* QoS level of message */
    UCHAR retain;       /* Retain flag */
} NXD_MQTT_MESSAGE;
/* Create a message queue */
TX_QUEUE measurement_queue;
UCHAR measurement_queue_buffer[MEASUREMENT_QUEUE_DEPTH * MEASUREMENT_QUEUE_MSG_SIZE];


#define QUEUE_DEPTH 10
#define QUEUE_MSG_SIZE 256

TX_EVENT_FLAGS_GROUP     SntpFlags;

ULONG mqtt_client_stack[MQTT_CLIENT_STACK_SIZE];

TX_EVENT_FLAGS_GROUP mqtt_app_flag;

/* Declare buffers to hold message and topic. */
static char message[NXD_MQTT_MAX_MESSAGE_LENGTH];
static UCHAR message_buffer[NXD_MQTT_MAX_MESSAGE_LENGTH];
static UCHAR topic_buffer[NXD_MQTT_MAX_TOPIC_NAME_LENGTH];
/* TLS buffers and certificate containers. */
//extern const NX_SECURE_TLS_CRYPTO nx_crypto_tls_ciphers;
/* calculated with nx_secure_tls_metadata_size_calculate */
//static CHAR crypto_metadata_client[11600];
/* Define the TLS packet reassembly buffer. */
UCHAR tls_packet_buffer[4000];
ULONG current_time;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID nx_app_thread_entry (ULONG thread_input);
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr);
/* USER CODE BEGIN PFP */
static VOID App_MQTT_Client_Thread_Entry(ULONG thread_input);
static VOID App_SNTP_Thread_Entry(ULONG thread_input);
static VOID App_Link_Thread_Entry(ULONG thread_input);

static VOID time_update_callback(NX_SNTP_TIME_MESSAGE *time_update_ptr, NX_SNTP_TIME *local_time);
//static ULONG nx_secure_tls_session_time_function(void);
static UINT dns_create(NX_DNS *dns_ptr);

/*static UINT tls_setup_callback(NXD_MQTT_CLIENT *client_pt,
                        NX_SECURE_TLS_SESSION *TLS_session_ptr,
                        NX_SECURE_X509_CERT *certificate_ptr,
                        NX_SECURE_X509_CERT *trusted_certificate_ptr);
*/
/* USER CODE END PFP */

static void SetRtcFromEpoch(uint32_t epoch);
static uint32_t GetRtcEpoch();


#define CACHE_OP(__op__) do { \
  if (is_cache_enable()) { \
    __op__; \
  } \
} while (0)

#define ALIGN_VALUE(_v_,_a_) (((_v_) + (_a_) - 1) & ~((_a_) - 1))

#define LCD_FG_WIDTH LCD_BG_WIDTH
#define LCD_FG_HEIGHT LCD_BG_HEIGHT

#define NUMBER_COLORS 10
#define BQUEUE_MAX_BUFFERS 3
#define CPU_LOAD_HISTORY_DEPTH 8

#define CIRCLE_RADIUS 5
/* Must be odd */
#define BINDING_WIDTH 3
#define COLOR_HEAD UTIL_LCD_COLOR_GREEN
#define COLOR_ARMS UTIL_LCD_COLOR_BLUE
#define COLOR_TRUNK UTIL_LCD_COLOR_MAGENTA
#define COLOR_LEGS UTIL_LCD_COLOR_ORANGE
#define COLOR_BOX UTIL_LCD_COLOR_RED

#define DISPLAY_BUFFER_NB (DISPLAY_DELAY + 2)

/* Align so we are sure nn_output_buffers[0] and nn_output_buffers[1] are aligned on 32 bytes */
#define NN_BUFFER_OUT_SIZE_ALIGN ALIGN_VALUE(NN_BUFFER_OUT_SIZE, 32)

typedef struct
{
  uint32_t X0;
  uint32_t Y0;
  uint32_t XSize;
  uint32_t YSize;
} Rectangle_TypeDef;

typedef struct {
  TX_SEMAPHORE free;
  TX_SEMAPHORE ready;
  int buffer_nb;
  uint8_t *buffers[BQUEUE_MAX_BUFFERS];
  int free_idx;
  int ready_idx;
} bqueue_t;

typedef struct {
  uint64_t current_total;
  uint64_t current_thread_total;
  uint64_t prev_total;
  uint64_t prev_thread_total;
  struct {
    uint64_t total;
    uint64_t thread;
    uint32_t tick;
  } history[CPU_LOAD_HISTORY_DEPTH];
} cpuload_info_t;

typedef struct {
  int32_t nb_detect;
  mpe_pp_outBuffer_t detects[AI_MPE_YOLOV8_PP_MAX_BOXES_LIMIT];
  uint32_t nn_period_ms;
  uint32_t inf_ms;
  uint32_t pp_ms;
  uint32_t disp_ms;
} display_info_t;

typedef struct {
  TX_SEMAPHORE update;
  TX_MUTEX lock;
  display_info_t info;
} display_t;

/* Globals */
DECLARE_CLASSES_TABLE;
/* Lcd Background area */
static Rectangle_TypeDef lcd_bg_area = {
  .X0 = (LCD_DEFAULT_WIDTH - LCD_BG_WIDTH) / 2,
  .Y0 = (LCD_DEFAULT_HEIGHT - LCD_BG_HEIGHT) / 2,
  .XSize = LCD_BG_WIDTH,
  .YSize = LCD_BG_HEIGHT,
};
/* Lcd Foreground area */
static Rectangle_TypeDef lcd_fg_area = {
  .X0 = (LCD_DEFAULT_WIDTH - LCD_FG_WIDTH) / 2,
  .Y0 = (LCD_DEFAULT_HEIGHT - LCD_FG_HEIGHT) / 2,
  .XSize = LCD_FG_WIDTH,
  .YSize = LCD_FG_HEIGHT,
};
static const uint32_t colors[NUMBER_COLORS] = {
    UTIL_LCD_COLOR_GREEN,
    UTIL_LCD_COLOR_RED,
    UTIL_LCD_COLOR_CYAN,
    UTIL_LCD_COLOR_MAGENTA,
    UTIL_LCD_COLOR_YELLOW,
    UTIL_LCD_COLOR_GRAY,
    UTIL_LCD_COLOR_BLACK,
    UTIL_LCD_COLOR_BROWN,
    UTIL_LCD_COLOR_BLUE,
    UTIL_LCD_COLOR_ORANGE
};

static const int bindings[][3] = {
  {15, 13, COLOR_LEGS},
  {13, 11, COLOR_LEGS},
  {16, 14, COLOR_LEGS},
  {14, 12, COLOR_LEGS},
  {11, 12, COLOR_TRUNK},
  { 5, 11, COLOR_TRUNK},
  { 6, 12, COLOR_TRUNK},
  { 5,  6, COLOR_ARMS},
  { 5,  7, COLOR_ARMS},
  { 6,  8, COLOR_ARMS},
  { 7,  9, COLOR_ARMS},
  { 8, 10, COLOR_ARMS},
  { 1,  2, COLOR_HEAD},
  { 0,  1, COLOR_HEAD},
  { 0,  2, COLOR_HEAD},
  { 1,  3, COLOR_HEAD},
  { 2,  4, COLOR_HEAD},
  { 3,  5, COLOR_HEAD},
  { 4,  6, COLOR_HEAD},
};

static const int kp_color[17] = {
COLOR_HEAD,
COLOR_HEAD,
COLOR_HEAD,
COLOR_HEAD,
COLOR_HEAD,
COLOR_ARMS,
COLOR_ARMS,
COLOR_ARMS,
COLOR_ARMS,
COLOR_ARMS,
COLOR_ARMS,
COLOR_TRUNK,
COLOR_TRUNK,
COLOR_LEGS,
COLOR_LEGS,
COLOR_LEGS,
COLOR_LEGS,
};

/* Lcd Background Buffer */
static uint8_t lcd_bg_buffer[DISPLAY_BUFFER_NB][LCD_BG_WIDTH * LCD_BG_HEIGHT * 2] ALIGN_32 IN_PSRAM;
static int lcd_bg_buffer_disp_idx = 1;
static int lcd_bg_buffer_capt_idx = 0;
/* Lcd Foreground Buffer */
static uint8_t lcd_fg_buffer[2][LCD_FG_WIDTH * LCD_FG_HEIGHT* 2] ALIGN_32 IN_PSRAM;
static int lcd_fg_buffer_rd_idx;
static display_t disp;
static cpuload_info_t cpu_load;

/* model */
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Default);
 /* nn input buffers */
static uint8_t nn_input_buffers[3][NN_WIDTH * NN_HEIGHT * NN_BPP] ALIGN_32 IN_PSRAM;
static bqueue_t nn_input_queue;
 /* nn output buffers */
static uint8_t nn_output_buffers[2][NN_BUFFER_OUT_SIZE_ALIGN] ALIGN_32;
static bqueue_t nn_output_queue;

 /* threads */
  /* nn thread */
static TX_THREAD nn_thread;
static uint8_t nn_tread_stack[4096];
  /* pp + display thread */
static TX_THREAD pp_thread;
static uint8_t pp_tread_stack[4096];
  /* display thread */
static TX_THREAD dp_thread;
static uint8_t dp_tread_stack[4096];
  /* isp thread */
static TX_THREAD isp_thread;
static uint8_t isp_tread_stack[4096];
static TX_SEMAPHORE isp_sem;

#define FRAME_HISTORY 10
static mpe_pp_keyPoints_t nose_history[FRAME_HISTORY][AI_MPE_YOLOV8_PP_MAX_BOXES_LIMIT];
static uint8_t frame_index = 0;
static int initialized_frames = 0;



static int is_cache_enable()
{
#if defined(USE_DCACHE)
  return 1;
#else
  return 0;
#endif
}

static void cpuload_init(cpuload_info_t *cpu_load)
{
  memset(cpu_load, 0, sizeof(cpuload_info_t));
}

static void cpuload_update(cpuload_info_t *cpu_load)
{
  EXECUTION_TIME thread_total;
  EXECUTION_TIME isr;
  EXECUTION_TIME idle;
  int i;

  cpu_load->history[1] = cpu_load->history[0];

  _tx_execution_thread_total_time_get(&thread_total);
  _tx_execution_isr_time_get(&isr);
  _tx_execution_idle_time_get(&idle);

  cpu_load->history[0].total = thread_total + isr + idle;
  cpu_load->history[0].thread = thread_total;
  cpu_load->history[0].tick = HAL_GetTick();

  if (cpu_load->history[1].tick - cpu_load->history[2].tick < 1000)
    return ;

  for (i = 0; i < CPU_LOAD_HISTORY_DEPTH - 2; i++)
    cpu_load->history[CPU_LOAD_HISTORY_DEPTH - 1 - i] = cpu_load->history[CPU_LOAD_HISTORY_DEPTH - 1 - i - 1];
}

static void cpuload_get_info(cpuload_info_t *cpu_load, float *cpu_load_last, float *cpu_load_last_second,
                             float *cpu_load_last_five_seconds)
{
  if (cpu_load_last)
    *cpu_load_last = 100.0 * (cpu_load->history[0].thread - cpu_load->history[1].thread) /
                     (cpu_load->history[0].total - cpu_load->history[1].total);
  if (cpu_load_last_second)
    *cpu_load_last_second = 100.0 * (cpu_load->history[2].thread - cpu_load->history[3].thread) /
                     (cpu_load->history[2].total - cpu_load->history[3].total);
  if (cpu_load_last_five_seconds)
    *cpu_load_last_five_seconds = 100.0 * (cpu_load->history[2].thread - cpu_load->history[7].thread) /
                     (cpu_load->history[2].total - cpu_load->history[7].total);
}

static int bqueue_init(bqueue_t *bq, int buffer_nb, uint8_t **buffers)
{
  int ret;
  int i;

  if (buffer_nb > BQUEUE_MAX_BUFFERS)
    return -1;

  ret = tx_semaphore_create(&bq->free, NULL, buffer_nb);
  if (ret)
    goto free_sem_error;
  ret = tx_semaphore_create(&bq->ready, NULL, 0);
  if (ret)
    goto ready_sem_error;

  bq->buffer_nb = buffer_nb;
  for (i = 0; i < buffer_nb; i++) {
    assert(buffers[i]);
    bq->buffers[i] = buffers[i];
  }
  bq->free_idx = 0;
  bq->ready_idx = 0;

  return 0;

ready_sem_error:
  tx_semaphore_delete(&bq->free);
free_sem_error:
  return -1;
}

static uint8_t *bqueue_get_free(bqueue_t *bq, int is_blocking)
{
  uint8_t *res;
  int ret;

  ret = tx_semaphore_get(&bq->free, is_blocking ? TX_WAIT_FOREVER : TX_NO_WAIT);
  if (ret == TX_NO_INSTANCE)
    return NULL;
  assert(ret == 0);

  res = bq->buffers[bq->free_idx];
  bq->free_idx = (bq->free_idx + 1) % bq->buffer_nb;

  return res;
}

static void bqueue_put_free(bqueue_t *bq)
{
  int ret;

  ret = tx_semaphore_put(&bq->free);
  assert(ret == 0);
}

static uint8_t *bqueue_get_ready(bqueue_t *bq)
{
  uint8_t *res;
  int ret;

  ret = tx_semaphore_get(&bq->ready, TX_WAIT_FOREVER);
  assert(ret == 0);

  res = bq->buffers[bq->ready_idx];
  bq->ready_idx = (bq->ready_idx + 1) % bq->buffer_nb;

  return res;
}

static void bqueue_put_ready(bqueue_t *bq)
{
  int ret;

  ret = tx_semaphore_put(&bq->ready);
  assert(ret == 0);
}

static void app_main_pipe_frame_event()
{
  int next_disp_idx = (lcd_bg_buffer_disp_idx + 1) % DISPLAY_BUFFER_NB;
  int next_capt_idx = (lcd_bg_buffer_capt_idx + 1) % DISPLAY_BUFFER_NB;
  int ret;

  ret = HAL_DCMIPP_PIPE_SetMemoryAddress(CMW_CAMERA_GetDCMIPPHandle(), DCMIPP_PIPE1,
                                         DCMIPP_MEMORY_ADDRESS_0, (uint32_t) lcd_bg_buffer[next_capt_idx]);
  assert(ret == HAL_OK);

  ret = HAL_LTDC_SetAddress_NoReload(&hlcd_ltdc, (uint32_t) lcd_bg_buffer[next_disp_idx], LTDC_LAYER_1);
  assert(ret == HAL_OK);
  ret = HAL_LTDC_ReloadLayer(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING, LTDC_LAYER_1);
  assert(ret == HAL_OK);
  lcd_bg_buffer_disp_idx = next_disp_idx;
  lcd_bg_buffer_capt_idx = next_capt_idx;
}

static void app_ancillary_pipe_frame_event()
{
  uint8_t *next_buffer;
  int ret;

  next_buffer = bqueue_get_free(&nn_input_queue, 0);
  if (next_buffer) {
    ret = HAL_DCMIPP_PIPE_SetMemoryAddress(CMW_CAMERA_GetDCMIPPHandle(), DCMIPP_PIPE2,
                                           DCMIPP_MEMORY_ADDRESS_0, (uint32_t) next_buffer);
    assert(ret == HAL_OK);
    bqueue_put_ready(&nn_input_queue);
  }
}

static void app_main_pipe_vsync_event()
{
  int ret;

  ret = tx_semaphore_put(&isp_sem);
  assert(ret == 0);
}

static void LCD_init()
{
  BSP_LCD_LayerConfig_t LayerConfig = {0};

  BSP_LCD_Init(0, LCD_ORIENTATION_LANDSCAPE);

  /* Preview layer Init */
  LayerConfig.X0          = lcd_bg_area.X0;
  LayerConfig.Y0          = lcd_bg_area.Y0;
  LayerConfig.X1          = lcd_bg_area.X0 + lcd_bg_area.XSize;
  LayerConfig.Y1          = lcd_bg_area.Y0 + lcd_bg_area.YSize;
  LayerConfig.PixelFormat = LCD_PIXEL_FORMAT_RGB565;
  LayerConfig.Address     = (uint32_t) lcd_bg_buffer[lcd_bg_buffer_disp_idx];

  BSP_LCD_ConfigLayer(0, LTDC_LAYER_1, &LayerConfig);

  LayerConfig.X0 = lcd_fg_area.X0;
  LayerConfig.Y0 = lcd_fg_area.Y0;
  LayerConfig.X1 = lcd_fg_area.X0 + lcd_fg_area.XSize;
  LayerConfig.Y1 = lcd_fg_area.Y0 + lcd_fg_area.YSize;
  LayerConfig.PixelFormat = LCD_PIXEL_FORMAT_ARGB4444;
  LayerConfig.Address = (uint32_t) lcd_fg_buffer[1]; /* External XSPI1 PSRAM */

  BSP_LCD_ConfigLayer(0, LTDC_LAYER_2, &LayerConfig);
  UTIL_LCD_SetFuncDriver(&LCD_Driver);
  UTIL_LCD_SetLayer(LTDC_LAYER_2);
  UTIL_LCD_Clear(0x00000000);
  UTIL_LCD_SetFont(&Font20);
  UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
}

static int clamp_point(int *x, int *y)
{
  int xi = *x;
  int yi = *y;

  if (*x < 0)
    *x = 0;
  if (*y < 0)
    *y = 0;
  if (*x >= lcd_bg_area.XSize)
    *x = lcd_bg_area.XSize - 1;
  if (*y >= lcd_bg_area.YSize)
    *y = lcd_bg_area.YSize - 1;

  return (xi != *x) || (yi != *y);
}

static void convert_length(float32_t wi, float32_t hi, int *wo, int *ho)
{
  *wo = (int) (lcd_bg_area.XSize * wi);
  *ho = (int) (lcd_bg_area.YSize * hi);
}

static void convert_point(float32_t xi, float32_t yi, int *xo, int *yo)
{
  *xo = (int) (lcd_bg_area.XSize * xi);
  *yo = (int) (lcd_bg_area.YSize * yi);
}

static void Display_keypoint(mpe_pp_keyPoints_t *key, uint32_t color)
{
  int is_clamp;
  int xc, yc;
  int x, y;

  if (key->conf < AI_MPE_YOLOV8_PP_CONF_THRESHOLD)
    return ;

  convert_point(key->x, key->y, &x, &y);
  xc = x - CIRCLE_RADIUS / 2;
  yc = y - CIRCLE_RADIUS / 2;
  is_clamp = clamp_point(&xc, &yc);
  xc = x + CIRCLE_RADIUS / 2;
  yc = y + CIRCLE_RADIUS / 2;
  is_clamp |= clamp_point(&xc, &yc);

  if (is_clamp)
    return ;

  UTIL_LCD_FillCircle(x, y, CIRCLE_RADIUS, color);
}

static void Display_binding_line(int x0, int y0, int x1, int y1, uint32_t color)
{
  clamp_point(&x0, &y0);
  clamp_point(&x1, &y1);

  UTIL_LCD_DrawLine(x0, y0, x1, y1, color);
}

static void Display_binding(mpe_pp_keyPoints_t *from, mpe_pp_keyPoints_t *to, uint32_t color)
{
  int is_clamp;
  int x0, y0;
  int x1, y1;
  int i;

  assert(BINDING_WIDTH % 2 == 1);

  if (from->conf < AI_MPE_YOLOV8_PP_CONF_THRESHOLD)
    return ;
  if (to->conf < AI_MPE_YOLOV8_PP_CONF_THRESHOLD)
    return ;

  convert_point(from->x, from->y, &x0, &y0);
  is_clamp = clamp_point(&x0, &y0);
  if (is_clamp)
    return ;

  convert_point(to->x, to->y, &x1, &y1);
  is_clamp = clamp_point(&x1, &y1);
  if (is_clamp)
    return ;

  UTIL_LCD_DrawLine(x0, y0, x1, y1, color);
  for (i = 1; i <= (BINDING_WIDTH - 1) / 2; i++) {
    if (abs(y1 - y0) > abs(x1 - x0)) {
      Display_binding_line(x0 + i, y0, x1 + i , y1, color);
      Display_binding_line(x0 - i, y0, x1 - i , y1, color);
    } else {
      Display_binding_line(x0, y0 + i, x1 , y1 + i, color);
      Display_binding_line(x0, y0 - i, x1 , y1 - i, color);
    }
  }
}

static void Display_Detection(mpe_pp_outBuffer_t *detect)
{
  int xc, yc;
  int x0, y0;
  int x1, y1;
  int w, h;
  int i;

  convert_point(detect->x_center, detect->y_center, &xc, &yc);
  convert_length(detect->width, detect->height, &w, &h);
  x0 = xc - (w + 1) / 2;
  y0 = yc - (h + 1) / 2;
  x1 = xc + (w + 1) / 2;
  y1 = yc + (h + 1) / 2;
  clamp_point(&x0, &y0);
  clamp_point(&x1, &y1);

  UTIL_LCD_DrawRect(x0, y0, x1 - x0, y1 - y0, colors[detect->class_index % NUMBER_COLORS]);
  UTIL_LCDEx_PrintfAt(x0, y0, LEFT_MODE, classes_table[detect->class_index]);

  for (i = 0; i < ARRAY_NB(bindings); i++)
    Display_binding(&detect->pKeyPoints[bindings[i][0]], &detect->pKeyPoints[bindings[i][1]], bindings[i][2]);
  for (i = 0; i < AI_POSE_PP_POSE_KEYPOINTS_NB; i++)
    Display_keypoint(&detect->pKeyPoints[i], kp_color[i]);
}

static void Display_NetworkOutput(display_info_t *info)
{
  mpe_pp_outBuffer_t *rois = info->detects;
  uint32_t nb_rois = info->nb_detect;
  float cpu_load_one_second;
  int line_nb = 0;
  float nn_fps;
  int i;
  sensor_data_t data;
  /* clear previous ui */
  UTIL_LCD_FillRect(lcd_fg_area.X0, lcd_fg_area.Y0, lcd_fg_area.XSize, lcd_fg_area.YSize, 0x00000000); /* Clear previous boxes */

  /* cpu load */
  cpuload_update(&cpu_load);
  cpuload_get_info(&cpu_load, NULL, &cpu_load_one_second, NULL);
  if ((GetRtcEpoch() - prev_timestamp) > 3)
    {
  	  prev_timestamp = GetRtcEpoch();
        if (nb_rois != prev_nb_person){
  	     printf("persons detected = %lu\n\r",nb_rois);
           prev_nb_person = nb_rois;
  	     /* Allocate the memory for MQTT client thread   */
  	     data.nb_detect = prev_nb_person;
  	     data.timestamp = GetRtcEpoch();
  	     data.state_detect = 1; //Normal person state
  	     prev_state_detect = 1;

  	     /* Send to MQTT thread */
  	     tx_queue_send(&measurement_queue, &data, TX_NO_WAIT);
  	  //tx_thread_sleep(1000);  // Adjust sampling rate as needed
  	  }
    }
  /* draw metrics */
  nn_fps = 1000.0 / info->nn_period_ms;
#if 1
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb),  RIGHT_MODE, "Cpu load");
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb),  RIGHT_MODE, "   %.1f%%", cpu_load_one_second);
  line_nb += 2;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "Inference");
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "   %ums", info->inf_ms);
  line_nb += 2;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "   FPS");
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "  %.2f", nn_fps);
  line_nb += 2;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, " Objects %u", nb_rois);
  line_nb += 1;
#else
  (void) nn_fps;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb),  RIGHT_MODE, "Cpu load");
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb),  RIGHT_MODE, "   %.1f%%", cpu_load_one_second);
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "nn period");
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "   %ums", info->nn_period_ms);
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "Inference");
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "   %ums", info->inf_ms);
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "Post process");
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "   %ums", info->pp_ms);
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "Display");
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "   %ums", info->disp_ms);
  line_nb += 1;
  UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, " Objects %u", nb_rois);
  line_nb += 1;
#endif
  /* Fall detection with 10-frame history */
      if(initialized_frames >= FRAME_HISTORY) {
          int hist_idx = (frame_index + FRAME_HISTORY - 10) % FRAME_HISTORY;

          for(int i = 0; i < info->nb_detect; i++) {
              mpe_pp_keyPoints_t curr_nose = info->detects[i].pKeyPoints[0];
              mpe_pp_keyPoints_t hist_nose = nose_history[hist_idx][i];

              if(curr_nose.conf > AI_MPE_YOLOV8_PP_CONF_THRESHOLD &&
                 hist_nose.conf > AI_MPE_YOLOV8_PP_CONF_THRESHOLD)
              {
                  // Convert coordinates
                  int x_curr, y_curr, x_hist, y_hist;
                  convert_point(curr_nose.x, curr_nose.y, &x_curr, &y_curr);
                  convert_point(hist_nose.x, hist_nose.y, &x_hist, &y_hist);

                  // Calculate motion vector
                  float dx = x_curr - x_hist;
                  float dy = y_curr - y_hist;
                  float length = sqrtf(dx*dx + dy*dy);

                  // Display vector length
                  int box_x0 = (int)(lcd_bg_area.XSize * (info->detects[i].x_center - info->detects[i].width/2));
                  int box_y0 = (int)(lcd_bg_area.YSize * (info->detects[i].y_center - info->detects[i].height/2));
                  UTIL_LCDEx_PrintfAt(box_x0, box_y0 - 20, LEFT_MODE, "%.0fpx", length);

                  // Fall detection (vertical movement >10px over 10 frames)
                  if(dy > 80) {
                     UTIL_LCDEx_PrintfAt(0, 10, LEFT_MODE, "Fall detected!");
               	     printf("Fall detected!\n\r");
               	     /* Allocate the memory for MQTT client thread   */
               	     data.nb_detect = nb_rois;
               	     data.timestamp = GetRtcEpoch();
               	     data.state_detect = 13; //Fall detected!
               	     /* Send to MQTT thread */
               	     if (prev_state_detect != 13){
               	     	 tx_queue_send(&measurement_queue, &data, TX_NO_WAIT);
               	     	 prev_state_detect = 13;
               	     }

                  }
              }
          }
      }

  /* Draw bounding boxes */
  for (i = 0; i < nb_rois; i++)
    Display_Detection(&rois[i]);

  // Update history buffer
      for(int i = 0; i < info->nb_detect; i++) {
          nose_history[frame_index][i] = info->detects[i].pKeyPoints[0];
      }
      frame_index = (frame_index + 1) % FRAME_HISTORY;
      if(initialized_frames < FRAME_HISTORY) initialized_frames++;
}

static void nn_thread_fct(ULONG arg)
{
  const LL_Buffer_InfoTypeDef *nn_out_info = LL_ATON_Output_Buffers_Info_Default();
  const LL_Buffer_InfoTypeDef *nn_in_info = LL_ATON_Input_Buffers_Info_Default();
  uint32_t nn_period_ms;
  uint32_t nn_period[2];
  uint8_t *nn_pipe_dst;
  uint32_t nn_out_len;
  uint32_t nn_in_len;
  uint32_t inf_ms;
  uint32_t ts;
  int ret;

  /* setup buffers size */
  nn_in_len = LL_Buffer_len(&nn_in_info[0]);
  nn_out_len = LL_Buffer_len(&nn_out_info[0]);
  //printf("nn_out_len = %d\n", nn_out_len);
  assert(nn_out_len == NN_BUFFER_OUT_SIZE);

  /*** App Loop ***************************************************************/
  nn_period[1] = HAL_GetTick();

  nn_pipe_dst = bqueue_get_free(&nn_input_queue, 0);
  assert(nn_pipe_dst);
  CAM_NNPipe_Start(nn_pipe_dst, CMW_MODE_CONTINUOUS);
  while (1)
  {
    uint8_t *capture_buffer;
    uint8_t *output_buffer;

    nn_period[0] = nn_period[1];
    nn_period[1] = HAL_GetTick();
    nn_period_ms = nn_period[1] - nn_period[0];

    capture_buffer = bqueue_get_ready(&nn_input_queue);
    assert(capture_buffer);
    output_buffer = bqueue_get_free(&nn_output_queue, 1);
    assert(output_buffer);

    /* run ATON inference */
    ts = HAL_GetTick();
     /* Note that we don't need to clean/invalidate those input buffers since they are only access in hardware */
    ret = LL_ATON_Set_User_Input_Buffer_Default(0, capture_buffer, nn_in_len);
    assert(ret == LL_ATON_User_IO_NOERROR);
     /* Invalidate output buffer before Hw access it */
    CACHE_OP(SCB_InvalidateDCache_by_Addr(output_buffer, nn_out_len));
    ret = LL_ATON_Set_User_Output_Buffer_Default(0, output_buffer, nn_out_len);
    assert(ret == LL_ATON_User_IO_NOERROR);
    LL_ATON_RT_Main(&NN_Instance_Default);
    inf_ms = HAL_GetTick() - ts;

    /* release buffers */
    bqueue_put_free(&nn_input_queue);
    bqueue_put_ready(&nn_output_queue);

    /* update display stats */
    tx_mutex_get(&disp.lock, TX_WAIT_FOREVER);
    disp.info.inf_ms = inf_ms;
    disp.info.nn_period_ms = nn_period_ms;
    tx_mutex_put(&disp.lock);
  }
}

static void pp_thread_fct(ULONG arg)
{
#if POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V2_UF
  yolov2_pp_static_param_t pp_params;
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V5_UU
  yolov5_pp_static_param_t pp_params;
#elif POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UF || POSTPROCESS_TYPE == POSTPROCESS_OD_YOLO_V8_UI
  yolov8_pp_static_param_t pp_params;
#elif POSTPROCESS_TYPE == POSTPROCESS_POSE_YOLO_V8_UF
  yolov8_pose_pp_static_param_t pp_params;
#elif POSTPROCESS_TYPE == POSTPROCESS_MPE_YOLO_V8_UF
  mpe_yolov8_pp_static_param_t pp_params;
#else
    #error "PostProcessing type not supported"
#endif
#if POSTPROCESS_TYPE == POSTPROCESS_MPE_YOLO_V8_UF
  mpe_pp_out_t pp_output;
#else
  postprocess_out_t pp_output;
#endif
  uint32_t nn_pp[2];
  void *pp_input;
  int ret;
  int i;

  /* setup post process */
  app_postprocess_init(&pp_params);
  while (1)
  {
    uint8_t *output_buffer;

    output_buffer = bqueue_get_ready(&nn_output_queue);
    assert(output_buffer);
    pp_input = (void *) output_buffer;
    pp_output.pOutBuff = NULL;

    nn_pp[0] = HAL_GetTick();
    ret = app_postprocess_run((void * []){pp_input}, 1, &pp_output, &pp_params);
    assert(ret == 0);
    nn_pp[1] = HAL_GetTick();

    /* update display stats and detection info */
    tx_mutex_get(&disp.lock, TX_WAIT_FOREVER);
    disp.info.nb_detect = pp_output.nb_detect;
    for (i = 0; i < pp_output.nb_detect; i++)
      disp.info.detects[i] = pp_output.pOutBuff[i];
    disp.info.pp_ms = nn_pp[1] - nn_pp[0];
    tx_mutex_put(&disp.lock);

    bqueue_put_free(&nn_output_queue);
    tx_semaphore_ceiling_put(&disp.update, 1);
  }
}

static void dp_update_drawing_area()
{
  int ret;

  __disable_irq();
  ret = HAL_LTDC_SetAddress_NoReload(&hlcd_ltdc, (uint32_t) lcd_fg_buffer[lcd_fg_buffer_rd_idx], LTDC_LAYER_2);
  assert(ret == HAL_OK);
  __enable_irq();
}

static void dp_commit_drawing_area()
{
  int ret;

  __disable_irq();
  ret = HAL_LTDC_ReloadLayer(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING, LTDC_LAYER_2);
  assert(ret == HAL_OK);
  __enable_irq();
  lcd_fg_buffer_rd_idx = 1 - lcd_fg_buffer_rd_idx;
}

static void dp_thread_fct(ULONG arg)
{
  uint32_t disp_ms = 0;
  display_info_t info;
  uint32_t ts;
  int ret;

  while (1)
  {
    ret = tx_semaphore_get(&disp.update, TX_WAIT_FOREVER);
    assert(ret == 0);

    tx_mutex_get(&disp.lock, TX_WAIT_FOREVER);
    info = disp.info;
    tx_mutex_put(&disp.lock);
    info.disp_ms = disp_ms;

    ts = HAL_GetTick();
    dp_update_drawing_area();
    Display_NetworkOutput(&info);
    SCB_CleanDCache_by_Addr(lcd_fg_buffer[lcd_fg_buffer_rd_idx], LCD_FG_WIDTH * LCD_FG_HEIGHT* 2);
    dp_commit_drawing_area();
    disp_ms = HAL_GetTick() - ts;
  }
}

static void isp_thread_fct(ULONG arg)
{
  int ret;

  while (1) {
    ret = tx_semaphore_get(&isp_sem, TX_WAIT_FOREVER);
    assert(ret == 0);

    CAM_IspUpdate();
  }
}

void app_run()
{
  const UINT isp_priority = TX_MAX_PRIORITIES / 2 - 2;
  const UINT pp_priority = TX_MAX_PRIORITIES / 2 + 2;
  const UINT dp_priority = TX_MAX_PRIORITIES / 2 + 2;
  const UINT nn_priority = TX_MAX_PRIORITIES / 2 - 1;
  const ULONG time_slice = 10;
  int ret;

  printf("Init application\n\r");
  /* Enable DWT so DWT_CYCCNT works when debugger not attached */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  /* screen init */
  memset(lcd_bg_buffer, 0, sizeof(lcd_bg_buffer));
  CACHE_OP(SCB_CleanInvalidateDCache_by_Addr(lcd_bg_buffer, sizeof(lcd_bg_buffer)));
  memset(lcd_fg_buffer, 0, sizeof(lcd_fg_buffer));
  CACHE_OP(SCB_CleanInvalidateDCache_by_Addr(lcd_fg_buffer, sizeof(lcd_fg_buffer)));
  LCD_init();

  /* create buffer queues */
  ret = bqueue_init(&nn_input_queue, 3, (uint8_t *[3]){nn_input_buffers[0], nn_input_buffers[1], nn_input_buffers[2]});
  assert(ret == 0);
  ret = bqueue_init(&nn_output_queue, 2, (uint8_t *[2]){nn_output_buffers[0], nn_output_buffers[1]});
  assert(ret == 0);

  cpuload_init(&cpu_load);

  /*** Camera Init ************************************************************/  
  CAM_Init();

  /* sems + mutex init */
  ret = tx_semaphore_create(&isp_sem, NULL, 0);
  assert(ret == 0);
  ret = tx_semaphore_create(&disp.update, NULL, 0);
  assert(ret == 0);
  ret= tx_mutex_create(&disp.lock, NULL, TX_INHERIT);
  assert(ret == 0);

  /* Start LCD Display camera pipe stream */
  CAM_DisplayPipe_Start(lcd_bg_buffer[0], CMW_MODE_CONTINUOUS);

  /* threads init */
  ret = tx_thread_create(&nn_thread, "nn", nn_thread_fct, 0, nn_tread_stack,
                         sizeof(nn_tread_stack), nn_priority, nn_priority, time_slice, TX_AUTO_START);
  assert(ret == TX_SUCCESS);
  ret = tx_thread_create(&pp_thread, "pp", pp_thread_fct, 0, pp_tread_stack,
                         sizeof(pp_tread_stack), pp_priority, pp_priority, time_slice, TX_AUTO_START);
  assert(ret == TX_SUCCESS);
  ret = tx_thread_create(&dp_thread, "dp", dp_thread_fct, 0, dp_tread_stack,
                         sizeof(dp_tread_stack), dp_priority, dp_priority, time_slice, TX_AUTO_START);
  assert(ret == TX_SUCCESS);
  ret = tx_thread_create(&isp_thread, "isp", isp_thread_fct, 0, isp_tread_stack,
                         sizeof(isp_tread_stack), isp_priority, isp_priority, time_slice, TX_AUTO_START);
  assert(ret == TX_SUCCESS);
  UINT status = TX_SUCCESS;
  VOID *memory_ptr;

  if (tx_byte_pool_create(&tx_app_byte_pool, "Tx App memory pool", tx_byte_pool_buffer, TX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
  {
    Error_Handler();
  }
  else
  {
    memory_ptr = (VOID *)&tx_app_byte_pool;
    status = App_ThreadX_Init(memory_ptr);
    if (status != TX_SUCCESS)
    {
      Error_Handler();
    }
  }

  if (tx_byte_pool_create(&nx_app_byte_pool, "Nx App memory pool", nx_byte_pool_buffer, NX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
  {
    Error_Handler();
  }
  else
  {
    memory_ptr = (VOID *)&nx_app_byte_pool;
    status = MX_NetXDuo_Init(memory_ptr);
    if (status != NX_SUCCESS)
    {
      Error_Handler();
    }
  }
}

int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe)
{
  if (pipe == DCMIPP_PIPE1)
    app_main_pipe_frame_event();
  else if (pipe == DCMIPP_PIPE2)
    app_ancillary_pipe_frame_event();

  return HAL_OK;
}

int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe)
{
  if (pipe == DCMIPP_PIPE1)
    app_main_pipe_vsync_event();

  return HAL_OK;
}
/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  CHAR *pointer;
  /* Initialize the NetXDuo system. */
  nx_system_initialize();

    /* Allocate the memory for packet_pool.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  ret = nx_packet_pool_create(&NxAppPool, "NetXDuo App Pool", DEFAULT_PAYLOAD_SIZE, pointer, NX_APP_PACKET_POOL_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_POOL_ERROR;
  }

    /* Allocate the memory for Ip_Instance */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, Nx_IP_INSTANCE_THREAD_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

   /* Create the main NX_IP instance */
  ret = nx_ip_create(&NetXDuoEthIpInstance, "NetX Ip instance", NX_APP_DEFAULT_IP_ADDRESS, NX_APP_DEFAULT_NET_MASK, &NxAppPool, nx_stm32_eth_driver,
                     pointer, Nx_IP_INSTANCE_THREAD_SIZE, NX_APP_INSTANCE_PRIORITY);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

    /* Allocate the memory for ARP */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, DEFAULT_ARP_CACHE_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  ret = nx_arp_enable(&NetXDuoEthIpInstance, (VOID *)pointer, DEFAULT_ARP_CACHE_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }
  ret = nx_icmp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  ret = nx_tcp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  ret = nx_udp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }
  ret = nx_dhcp_create(&DHCPClient, &NetXDuoEthIpInstance, "DHCP Client");

  if (ret != NX_SUCCESS)
  {
      return NX_DHCP_ERROR;
  }

  /* set DHCP notification callback  */
  tx_semaphore_create(&DHCPSemaphore, "DHCP Semaphore", 0);

  /* Allocate the memory for main thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main thread */
  ret = tx_thread_create(&NxAppThread, "NetXDuo App thread", nx_app_thread_entry , 0, pointer, NX_APP_THREAD_STACK_SIZE,
                         NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* USER CODE BEGIN MX_NetXDuo_Init */
  printf("Nx_MQTT_Client application started..\n\r");

  /* Allocate the memory for SNTP client thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, SNTP_CLIENT_THREAD_MEMORY, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* create the SNTP client thread */
  ret = tx_thread_create(&AppSNTPThread, "App SNTP Thread", App_SNTP_Thread_Entry, 0, pointer, SNTP_CLIENT_THREAD_MEMORY,
                         SNTP_PRIORITY, SNTP_PRIORITY, TX_NO_TIME_SLICE, TX_DONT_START);

  if (ret != TX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* Create the event flags. */
  ret = tx_event_flags_create(&SntpFlags, "SNTP event flags");

  /* Check for errors */
  if (ret != TX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* Allocate the memory for MQTT client thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, THREAD_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* create the MQTT client thread */
  ret = tx_thread_create(&AppMQTTClientThread, "App MQTT Thread", App_MQTT_Client_Thread_Entry, 0, pointer, THREAD_MEMORY_SIZE,
                         MQTT_PRIORITY, MQTT_PRIORITY, TX_NO_TIME_SLICE, TX_DONT_START);

  if (ret != TX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* Allocate the memory for Link thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,2 *  DEFAULT_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* create the Link thread */
  ret = tx_thread_create(&AppLinkThread, "App Link Thread", App_Link_Thread_Entry, 0, pointer, 2 * DEFAULT_MEMORY_SIZE,
                         LINK_PRIORITY, LINK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }


  /* Allocate the MsgQueueOne.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, APP_QUEUE_SIZE*sizeof(ULONG), TX_NO_WAIT) != TX_SUCCESS)
  {
    ret = TX_POOL_ERROR;
  }

  /* Create the MsgQueueOne shared by MsgSenderThreadOne and MsgReceiverThread */
  if (tx_queue_create(&MsgQueueOne, "Message Queue One",TX_1_ULONG, pointer, APP_QUEUE_SIZE*sizeof(ULONG)) != TX_SUCCESS)
  {
    ret = TX_QUEUE_ERROR;
  }

  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}

/**
* @brief  ip address change callback.
* @param ip_instance: NX_IP instance
* @param ptr: user data
* @retval none
*/
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
  /* release the semaphore as soon as an IP address is available */
  if (nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask) != NX_SUCCESS)
  {
    /* USER CODE BEGIN IP address change callback error */
    Error_Handler();
    /* USER CODE END IP address change callback error */
  }
  if(IpAddress != NULL_ADDRESS)
  {
    tx_semaphore_put(&DHCPSemaphore);
  }
}

/**
* @brief  Main thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID nx_app_thread_entry (ULONG thread_input)
{
  UINT ret = NX_SUCCESS;

  /* Create a DNS client */
  ret = dns_create(&DnsClient);

  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }
  /* register the IP address change callback */
  ret = nx_ip_address_change_notify(&NetXDuoEthIpInstance, ip_address_change_notify_callback, NULL);
  if (ret != NX_SUCCESS)
  {
    /* USER CODE BEGIN IP address change callback error */
    Error_Handler();
    /* USER CODE END IP address change callback error */
  }

  /* set DHCP notification callback  */
   /*start the DHCP client */
  ret = nx_dhcp_start(&DHCPClient);
  if (ret != NX_SUCCESS)
  {

    Error_Handler();

  }

  printf("Looking for DHCP server ..\n\r");
  if(tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
  {
	  printf("Looking for DHCP server ..error\n\r");
    Error_Handler();
  }

  /* USER CODE BEGIN Nx_App_Thread_Entry 2 */
  PRINT_IP_ADDRESS(IpAddress);

  /* start the SNTP client thread */
  tx_thread_resume(&AppSNTPThread);

  /* this thread is not needed any more, we relinquish it */
  tx_thread_relinquish();
  /* USER CODE END Nx_App_Thread_Entry 2 */

}
/* USER CODE BEGIN 1 */

/**
* @brief  DNS Create Function.
* @param dns_ptr
* @retval ret
*/
UINT dns_create(NX_DNS *dns_ptr)
{
  UINT ret = NX_SUCCESS;

  /* Create a DNS instance for the Client */
  ret = nx_dns_create(dns_ptr, &NetXDuoEthIpInstance, (UCHAR *)"DNS Client");
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }
  /* Initialize DNS instance with a dummy server */
  ret = nx_dns_server_add(dns_ptr, USER_DNS_ADDRESS);
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  return ret;
}

/* Declare the disconnect notify function. */
static VOID my_disconnect_func(NXD_MQTT_CLIENT *client_ptr)
{
  NX_PARAMETER_NOT_USED(client_ptr);
  printf("client disconnected from broker < %s >.\n\r", MQTT_BROKER_NAME);
}

/* Declare the notify function. */
static VOID my_notify_func(NXD_MQTT_CLIENT* client_ptr, UINT number_of_messages)
{
  NX_PARAMETER_NOT_USED(client_ptr);
  NX_PARAMETER_NOT_USED(number_of_messages);
  tx_event_flags_set(&mqtt_app_flag, DEMO_MESSAGE_EVENT, TX_OR);
  return;
}

/* Function (set by user) to call when TLS needs the current time. */
/*ULONG nx_secure_tls_session_time_function(void)
{
  return (current_time);
}*/

/* Callback to setup TLS parameters for secure MQTT connection. */
/*static UINT tls_setup_callback(NXD_MQTT_CLIENT *client_pt,
                        NX_SECURE_TLS_SESSION *TLS_session_ptr,
                        NX_SECURE_X509_CERT *certificate_ptr,
                        NX_SECURE_X509_CERT *trusted_certificate_ptr)
{
  UINT ret = NX_SUCCESS;
  NX_PARAMETER_NOT_USED(client_pt);
*/
  /* Initialize TLS module */
/*  nx_secure_tls_initialize();
*/
  /* Create a TLS session */
/*  ret = nx_secure_tls_session_create(TLS_session_ptr, &nx_crypto_tls_ciphers,
                                     crypto_metadata_client, sizeof(crypto_metadata_client));
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }
*/
  /* Need to allocate space for the certificate coming in from the broker. */
/*  memset((certificate_ptr), 0, sizeof(NX_SECURE_X509_CERT));

  ret = nx_secure_tls_session_time_function_set(TLS_session_ptr, nx_secure_tls_session_time_function);

  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }
*/
  /* Allocate space for packet reassembly. */
/*  ret = nx_secure_tls_session_packet_buffer_set(TLS_session_ptr, tls_packet_buffer,
                                                sizeof(tls_packet_buffer));
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }
*/
  /* allocate space for the certificate coming in from the remote host */
/*  ret = nx_secure_tls_remote_certificate_allocate(TLS_session_ptr, certificate_ptr,
                                                  tls_packet_buffer, sizeof(tls_packet_buffer));
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }
*/
  /* initialize Certificate to verify incoming server certificates. */
/*  ret = nx_secure_x509_certificate_initialize(trusted_certificate_ptr, (UCHAR*)mosquitto_org_der,
                                              mosquitto_org_der_len, NX_NULL, 0, NULL, 0,
                                              NX_SECURE_X509_KEY_TYPE_NONE);
  if (ret != NX_SUCCESS)
  {
    printf("Certificate issue..\nPlease make sure that your X509_certificate is valid. \n\r");
    Error_Handler();
  }
*/
  /* Add a CA Certificate to our trusted store */
/*
  ret = nx_secure_tls_trusted_certificate_add(TLS_session_ptr, trusted_certificate_ptr);
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  return ret;
}
*/
/* This callback defines a handler for notifying SNTP time update event.  */
static VOID time_update_callback(NX_SNTP_TIME_MESSAGE *time_update_ptr, NX_SNTP_TIME *local_time)
{
  NX_PARAMETER_NOT_USED(time_update_ptr);
  NX_PARAMETER_NOT_USED(local_time);

  tx_event_flags_set(&SntpFlags, SNTP_UPDATE_EVENT, TX_OR);
}

/** @brief  SNTP Client thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID App_SNTP_Thread_Entry(ULONG thread_input)
{
  UINT ret;
  ULONG  fraction;
  ULONG  events = 0;
  UINT   server_status;
  NXD_ADDRESS sntp_server_ip;

  sntp_server_ip.nxd_ip_version = 4;

  /* Look up SNTP Server address. */
  ret = nx_dns_host_by_name_get(&DnsClient, (UCHAR *)SNTP_SERVER_NAME, &sntp_server_ip.nxd_ip_address.v4, DEFAULT_TIMEOUT);

  /* Check for error. */
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  /* Create the SNTP Client */
  ret =  nx_sntp_client_create(&SntpClient, &NetXDuoEthIpInstance, 0, &NxAppPool, NULL, NULL, NULL);

  /* Check for error. */
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  /* Setup time update callback function. */
  nx_sntp_client_set_time_update_notify(&SntpClient, time_update_callback);

  /* Use the IPv4 service to set up the Client and set the IPv4 SNTP server. */
  ret = nx_sntp_client_initialize_unicast(&SntpClient, sntp_server_ip.nxd_ip_address.v4);

  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  /* Run whichever service the client is configured for. */
  ret = nx_sntp_client_run_unicast(&SntpClient);

  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  /* Wait for a server update event. */
  tx_event_flags_get(&SntpFlags, SNTP_UPDATE_EVENT, TX_OR_CLEAR, &events, PERIODIC_CHECK_INTERVAL);

  if (events == SNTP_UPDATE_EVENT)
  {
    /* Check for valid SNTP server status. */
    ret = nx_sntp_client_receiving_updates(&SntpClient, &server_status);

    if ((ret != NX_SUCCESS) || (server_status == NX_FALSE))
    {
      /* We do not have a valid update. */
      Error_Handler();
    }
    /* We have a valid update.  Get the SNTP Client time.  */
    ret = nx_sntp_client_get_local_time_extended(&SntpClient, &current_time, &fraction, NX_NULL, 0);

    if (ret != NX_SUCCESS)
    {
      Error_Handler();
    }
    /* take off 70 years difference */
    current_time -= EPOCH_TIME_DIFF;
    SetRtcFromEpoch(current_time);

  }
  else
  {
    Error_Handler();
  }

  /* start the MQTT client thread */
  tx_thread_resume(&AppMQTTClientThread);

}
/* Function to check if the client is connected */
UINT mqtt_client_is_connected(NXD_MQTT_CLIENT *client)
{
    return client->nxd_mqtt_client_state == NXD_MQTT_CLIENT_STATE_CONNECTED;
}

/**
* @brief  MQTT Client thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID App_MQTT_Client_Thread_Entry(ULONG thread_input)
{
  UINT ret = NX_SUCCESS;
  NXD_ADDRESS mqtt_server_ip;
  ULONG events;
  int len;
  UINT topic_length, message_length;
  UINT message_count = 0;
  sensor_data_t sensor_data;

  /* Initialize message queue */
  ret = tx_queue_create(&measurement_queue, "Measurement Queue",
                           MEASUREMENT_QUEUE_MSG_SIZE,
                           measurement_queue_buffer,  // Ensure you have a buffer
                           MEASUREMENT_QUEUE_DEPTH * MEASUREMENT_QUEUE_MSG_SIZE);
  if (ret != TX_SUCCESS) Error_Handler();
  mqtt_server_ip.nxd_ip_version = 4;

  /* Look up MQTT Server address. */
  ret = nx_dns_host_by_name_get(&DnsClient, (UCHAR *)MQTT_BROKER_NAME,
                                &mqtt_server_ip.nxd_ip_address.v4, DEFAULT_TIMEOUT);

  /* Check status.  */
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  /* Create MQTT client instance. */
  ret = nxd_mqtt_client_create(&MqttClient, "my_client", CLIENT_ID_STRING, STRLEN(CLIENT_ID_STRING),
                               &NetXDuoEthIpInstance, &NxAppPool, (VOID*)mqtt_client_stack, MQTT_CLIENT_STACK_SIZE,
                               MQTT_THREAD_PRIORTY, NX_NULL, 0);

  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  /* Register the disconnect notification function. */
  nxd_mqtt_client_disconnect_notify_set(&MqttClient, my_disconnect_func);

  /* Set the receive notify function. */
  nxd_mqtt_client_receive_notify_set(&MqttClient, my_notify_func);

  /* Create an MQTT flag */
  ret = tx_event_flags_create(&mqtt_app_flag, "my app event");
  if (ret != TX_SUCCESS)
  {
    Error_Handler();
  }

  //ret = nxd_mqtt_client_secure_connect(&MqttClient, &DefaultNXDAddress, MQTT_PORT, tls_setup_callback,MQTT_KEEP_ALIVE_TIMER, CLEAN_SESSION, NX_WAIT_FOREVER);
  ret = nxd_mqtt_client_connect(&MqttClient, &mqtt_server_ip, 1883,MQTT_KEEP_ALIVE_TIMER, CLEAN_SESSION, NX_WAIT_FOREVER);
  if (ret != NX_SUCCESS)
  {
    printf("\nMQTT client failed to connect to broker < %s >at PORT %d.\n\r",MQTT_BROKER_NAME, MQTT_PORT);
    Error_Handler();
  }
  else
  {
    printf("\nMQTT client connected to broker < %s > at PORT %d :\n\r",MQTT_BROKER_NAME, MQTT_PORT);
  }

  /* Subscribe to the topic with QoS level 1. */
  ret = nxd_mqtt_client_subscribe(&MqttClient, TOPIC_NAME, STRLEN(TOPIC_NAME), QOS1);
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }
  snprintf(message, sizeof(message),"{\"ts\":%lu,""\"mac\":\"%02X%02X%02X%02X%02X%02X\",""\"status\":\"start\"}",GetRtcEpoch(), MACAddr[0], MACAddr[1], MACAddr[2],MACAddr[3],MACAddr[4],MACAddr[5]);
  len = 0;
  while (message[len] != '\0') {
      len++;
  }
  printf("strlen %i\n\r",len);

  ret = nxd_mqtt_client_publish(&MqttClient, TOPIC_NAME, STRLEN(TOPIC_NAME),(CHAR*)message, len, NX_TRUE, QOS1, NX_WAIT_FOREVER);
      if (ret != NX_SUCCESS)
      {
        Error_Handler();
      }
  tx_event_flags_get(&mqtt_app_flag, DEMO_ALL_EVENTS, TX_OR_CLEAR, &events, TX_WAIT_FOREVER);

  while(1){
	/* Wait for measurement data from other thread */
	ret = tx_queue_receive(&measurement_queue, &sensor_data, TX_WAIT_FOREVER);
	if (ret != TX_SUCCESS) {
	       printf("Failed to receive data from queue: %u\n", ret);
	       continue;
	}
	/* Format JSON message */
	snprintf(message, sizeof(message),"{\"ts\":%lu,\"mac\":\"%02X%02X%02X%02X%02X%02X\",\"nb_detect\":%.0f,\"state_detect\":%i}",sensor_data.timestamp,MACAddr[0], MACAddr[1], MACAddr[2],MACAddr[3], MACAddr[4], MACAddr[5],sensor_data.nb_detect,sensor_data.state_detect);
    /* Publish data */
	len = 0;
	while (message[len] != '\0') {
	    len++;
	}
    /* Publish a message with QoS Level 1. */
    ret = nxd_mqtt_client_publish(&MqttClient, TOPIC_NAME, STRLEN(TOPIC_NAME),(CHAR*)message, len, NX_TRUE, QOS1, NX_WAIT_FOREVER);
    if (ret != NX_SUCCESS)
    {
      Error_Handler();
    }

    /* wait for the broker to publish the message. */
    tx_event_flags_get(&mqtt_app_flag, DEMO_ALL_EVENTS, TX_OR_CLEAR, &events, TX_WAIT_FOREVER);

    /* check event received */
    if(events & DEMO_MESSAGE_EVENT)
    {
      /* get message from the broker */
      ret = nxd_mqtt_client_message_get(&MqttClient, topic_buffer, sizeof(topic_buffer), &topic_length,message_buffer, sizeof(message_buffer), &message_length);
      if(ret == NXD_MQTT_SUCCESS)
      {
        printf("Message %d received: TOPIC = %s, MESSAGE = %s\n", message_count + 1, topic_buffer, message_buffer);
      }
      else
      {
        Error_Handler();
      }
    }

	tx_thread_sleep(100);
    }
}
/**
* @brief  MQTT Client thread log objects.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/

/**
* @brief  Link thread entry
* @param thread_input: ULONG thread parameter
* @retval none
*/
static VOID App_Link_Thread_Entry(ULONG thread_input)
{
  ULONG actual_status;
  UINT linkdown = 0, status;

  while(1)
  {
    /* Send request to check if the Ethernet cable is connected. */
    status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_LINK_ENABLED,
                                      &actual_status, 10);

    if(status == NX_SUCCESS)
    {
      if(linkdown == 1)
      {
        linkdown = 0;

        /* The network cable is connected. */
        printf("The network cable is connected.\n\r");

        /* Send request to enable PHY Link. */
        nx_ip_driver_direct_command(&NetXDuoEthIpInstance, NX_LINK_ENABLE,
                                      &actual_status);

        /* Send request to check if an address is resolved. */
        status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_ADDRESS_RESOLVED,
                                      &actual_status, 10);
        if(status == NX_SUCCESS)
        {
          /* Stop DHCP */
          nx_dhcp_stop(&DHCPClient);

          /* Reinitialize DHCP */
          nx_dhcp_reinitialize(&DHCPClient);

          /* Start DHCP */
          nx_dhcp_start(&DHCPClient);

          /* wait until an IP address is ready */
          if(tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
          {
            /* USER CODE BEGIN DHCPSemaphore get error */
            Error_Handler();
            /* USER CODE END DHCPSemaphore get error */
          }

          PRINT_IP_ADDRESS(IpAddress);
        }
        else
        {
          /* Set the DHCP Client's remaining lease time to 0 seconds to trigger an immediate renewal request for a DHCP address. */
          nx_dhcp_client_update_time_remaining(&DHCPClient, 0);
        }
      }
    }
    else
    {
      if(0 == linkdown)
      {
        linkdown = 1;
        /* The network cable is not connected. */
        printf("The network cable is not connected.\n\r");
        nx_ip_driver_direct_command(&NetXDuoEthIpInstance, NX_LINK_DISABLE,
                                      &actual_status);
      }
    }

    tx_thread_sleep(NX_APP_CABLE_CONNECTION_CHECK_PERIOD);
  }
}
#include <time.h>

static void SetRtcFromEpoch(uint32_t epoch) {
    RTC_DateTypeDef sDate = {0};
    RTC_TimeTypeDef sTime = {0};
    struct tm *tm_ptr = gmtime((time_t*)&epoch);  // Convert epoch to UTC time

    // Time structure
    sTime.Hours = tm_ptr->tm_hour;
    sTime.Minutes = tm_ptr->tm_min;
    sTime.Seconds = tm_ptr->tm_sec;
    sTime.TimeFormat = RTC_HOURFORMAT_24;

    // Date structure
    sDate.WeekDay = (tm_ptr->tm_wday == 0) ? 7 : tm_ptr->tm_wday;  // RTC: Mon=1, Sun=7
    sDate.Month = tm_ptr->tm_mon + 1;  // struct tm: 0-11 → RTC: 1-12
    sDate.Date = tm_ptr->tm_mday;
    sDate.Year = tm_ptr->tm_year - 100;  // struct tm: years since 1900 → RTC: years since 2000

    // Write to RTC
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}
static uint32_t GetRtcEpoch() {
    RTC_DateTypeDef sDate;
    RTC_TimeTypeDef sTime;
    struct tm tm_time = {0};

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    tm_time.tm_year = sDate.Year + 100;  // RTC → struct tm year offset
    tm_time.tm_mon = sDate.Month - 1;
    tm_time.tm_mday = sDate.Date;
    tm_time.tm_hour = sTime.Hours;
    tm_time.tm_min = sTime.Minutes;
    tm_time.tm_sec = sTime.Seconds;

    return (uint32_t)mktime(&tm_time);
}

