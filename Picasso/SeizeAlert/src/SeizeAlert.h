#pragma once

static void report_fall(void);
static void report_seizure(void);
static void init_seizure_datas(void);
static void deinit_seizure_datas(void);
static void timer_callback();
static void set_countdown();
static void countdown_callback();
void test_buffer_vals(void);
void display_countdown(int count);
float my_sqrt(const float num);
