#include <Arduino.h>
#include "esp_camera.h"
#include "fd_forward.h"
#include "fr_forward.h"
#include "fr_flash.h"

#define ENROLL_CONFIRM_TIMES 5
#define FACE_ID_SAVE_NUMBER 7

dl_matrix3du_t *image_matrix = NULL;
dl_matrix3du_t *aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
camera_fb_t *fb = NULL;
static face_id_list id_list = {0};

static inline mtmn_config_t app_mtmn_config()
{
  mtmn_config_t mtmn_config = {0};
  mtmn_config.type = FAST;
  mtmn_config.min_face = 80;
  mtmn_config.pyramid = 0.707;
  mtmn_config.pyramid_times = 4;
  mtmn_config.p_threshold.score = 0.6;
  mtmn_config.p_threshold.nms = 0.7;
  mtmn_config.p_threshold.candidate_number = 20;
  mtmn_config.r_threshold.score = 0.7;
  mtmn_config.r_threshold.nms = 0.7;
  mtmn_config.r_threshold.candidate_number = 10;
  mtmn_config.o_threshold.score = 0.7;
  mtmn_config.o_threshold.nms = 0.7;
  mtmn_config.o_threshold.candidate_number = 1;

  return mtmn_config;
}
mtmn_config_t mtmn_config = app_mtmn_config();

bool run_face_recognition()
{
  bool faceRecognised = false; // default
  int64_t start_time = esp_timer_get_time();
  fb = esp_camera_fb_get();
  if (!fb)
  {
    log_i("Camera capture failed");
    return false;
  }

  int64_t fb_get_time = esp_timer_get_time();
  log_i("Get one frame in %u ms.\n", (fb_get_time - start_time) / 1000); // this line can be commented out

  image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  uint32_t res = fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item);
  if (!res)
  {
    log_e("to rgb888 failed");
    dl_matrix3du_free(image_matrix);
  }

  esp_camera_fb_return(fb);

  box_array_t *net_boxes = face_detect(image_matrix, &mtmn_config);
  if (net_boxes)
  {
    if (align_face(net_boxes, image_matrix, aligned_face) == ESP_OK)
    {
      int matched_id = recognize_face(&id_list, aligned_face);
      if (matched_id >= 0)
      {
        log_i("Match Face ID: %u\n", matched_id);
        faceRecognised = true; // function will now return true
      }
      else
      {
        log_i("No Match Found");
        matched_id = -1;
      }
    }
    else
    {
      log_i("Face Not Aligned");
    }

    free(net_boxes->box);
    free(net_boxes->landmark);
    free(net_boxes);
  }

  dl_matrix3du_free(image_matrix);
  return faceRecognised;
}

void faceid_cleanup()
{
  face_id_init(&id_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
  const auto count = read_face_id_from_flash(&id_list);
  
  log_i("Deleting %u faces from flash", count);
  while (delete_face_id_in_flash(&id_list) > -1);

  log_i("All faces deleted");
}

void camera_setup()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    log_e("Camera init failed with error 0x%x", err);
    return;
  }

  //drop down frame size for higher initial frame rate
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  face_id_init(&id_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
  read_face_id_from_flash(&id_list); // Read current face data from on-board flash
}

void camera_loop(unsigned long now)
{
  if (run_face_recognition())
  { 
    // face recognition function has returned true
    // digitalWrite(relayPin, HIGH); //close (energise) relay
    // openedMillis = millis();      //time relay closed
    log_i("Face recognized!");
  }
}

void camera_thread_func(void*)
{
  camera_setup();

  while(true)
    camera_loop(millis());
}
