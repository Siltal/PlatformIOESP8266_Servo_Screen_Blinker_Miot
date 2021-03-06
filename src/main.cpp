#define BLINKER_DEBUG_ALL
#define BLINKER_WIFI
#define BLINKER_MIOT_LIGHT

#include <iostream>
#include <Arduino.h>
#include <Blinker.h>
#include <U8g2lib.h>
#include <Servo.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>


//定义显示屏接口
#define SCL 4
#define SDA 5
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

//定义舵机接口
Servo servo;
#define SVO1 D0                 //GPIO 16
#define SVO2 D5                 //GPIO 14
//#define DEFAULT_ON_ANGLE 199   //默认开启角度199
//#define DEFAULT_OFF_ANGLE 0    //默认关闭角度0

char auth[] = "80e641be7d58";   //Blinker authKey
//char bridge[]= "ffdd5e653703";
/**
 * 灯状态
 */
bool lamp_state;                //灯状态用于反转和反馈给小爱
bool oScreen = true;
bool reverse_state = false;     //反转模式，当控制开却关掉时使用
int count = 0;                  //计数器记录的数字
/**
 * 控制组件
 */
BlinkerButton btn_switch((char *) "switch");    //切换开关
BlinkerButton btn_open_lamp((char *) "btn_on_lamp");          //开实体灯
BlinkerButton btn_close_lamp((char *) "btn_off_lamp");        //关实体灯
BlinkerButton btn_open_led((char *) "btn_on_led");          //开LED灯
BlinkerButton btn_close_led((char *) "btn_off_led");        //关LED灯
BlinkerButton btn_close_screen((char *) "btn_close_screen");
BlinkerButton btn_plus((char *) "+1");                   //+1的按钮，反馈给计数器
BlinkerButton btn_reverse((char *) "btn_switch_reverse");//反转模式按钮
BlinkerNumber counter((char *) "counter");               //计数器控件
BlinkerButton btn_checker((char *) "checker");           //检查现在状态

BlinkerText text_reporter((char *) "reporter");           //报告现在状态


/**
 * 在显示屏上显示文本
 * @param str 文本
 */
void showText(const std::string &str) {
    if (!oScreen) return;
    u8g2.clearBuffer();          // clear the internal memory
    u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
    u8g2.drawStr(0, 10, str.c_str()); // 在固定坐标写入文本
    u8g2.sendBuffer();          // transfer internal memory to the display
}

/**
 * 关闭显示屏
 */
void close_screen() {
    u8g2.clearDisplay();
}


/**
 * 调整角度和显示角度
 * @param x 角度
 * @return 调整后的角度
 */
inline int adjust(int x) {
    showText(std::to_string(x));
    return x;
}

/**
 * 设置舵机1角度，以及延迟时间
 * @param pos 从0到199
 * @param _delay 延迟单位毫秒
 */
void set_servo_A_angle(int pos, int _delay) {
    servo.attach(SVO1);
    servo.write(adjust(pos));
    delay(_delay);
    servo.detach();
}

/**
 * 设置舵机2角度，以及延迟时间
 * @param pos 从0到199
 * @param _delay 延迟单位毫秒
 */
void set_servo_B_angle(int pos, int _delay) {
    servo.attach(SVO2);
    servo.write(adjust(pos));
    delay(_delay);
    servo.detach();
}

/**
 * 打开实体灯
 */
void open_entity_lamp() {
    set_servo_A_angle(reverse_state ? 150 : 40, 400);
    set_servo_A_angle(reverse_state ? 40 : 150, 400);
    lamp_state = true;
    btn_switch.print(BLINKER_CMD_ON);
    text_reporter.print(lamp_state ? "灯开着" : "灯关着");
}

/**
 * 关闭实体灯
 */
void close_entity_lamp() {
    set_servo_B_angle(reverse_state ? 40 : 150, 400);
    set_servo_B_angle(reverse_state ? 150 : 40, 400);
    lamp_state = false;
    btn_switch.print(BLINKER_CMD_OFF);
    text_reporter.print(lamp_state ? "灯开着" : "灯关着");
}

/**
 * 打开内置灯
 */
void open_buildin_led() {
    digitalWrite(LED_BUILTIN, LOW);
}

/**
 * 关闭内置灯
 */
void close_buildin_led() {
    digitalWrite(LED_BUILTIN, HIGH);
}

/***********************************************callback_function***********************************************/

/**
 * 打开led回调
 */
void open_led_callback(const String &state) {
    open_buildin_led();
    showText("open LED by btn_open_led");
}


/**
 * 关闭led 回调
 */
void close_led_callback(const String &state) {
    close_buildin_led();
    showText("close LED by btn_close_led");
}

/**
 * 检测状态回调
 */
void checker_callback(const String &state) {
    text_reporter.print(lamp_state ? "灯开着" : "灯关着");
}

/**
 * 开灯按钮回调函数
 * @param state on or off
 */
void open_lamp_callback(const String &state) {
    open_entity_lamp();
    showText("lamp opend<-btn_open_lamp");
}

/**
 * 关灯按钮回调函数
 * @param state on or off
 */
void close_lamp_callback(const String &state) {
    close_entity_lamp();
    showText("lamp closed<-btn_close_lamp");
}

/**
 * 切换灯状态回调函数
 * @param state
 */
void btn_switch_callback(const String &state) {
    if (lamp_state) {
        close_lamp_callback(state);
    } else {
        open_lamp_callback(state);
    }
    BlinkerMIOT.powerState(lamp_state ? BLINKER_CMD_ON : BLINKER_CMD_OFF);
    BlinkerMIOT.print();
}

/**
 * +1回调
 * @param state
 */
void plus_callback(const String &state) {
    if (state == "tap") {
        BLINKER_LOG("Blinker readString: ", state);
        count++;
        counter.print(count);
    }
}

/**
 * 开关反转状态
 * @param state on or off
 */
void reverse_callback(const String &state) {
    if (state == BLINKER_CMD_ON) {
        reverse_state = true;
        btn_reverse.print(BLINKER_CMD_ON);
    } else if (state == BLINKER_CMD_OFF) {
        reverse_state = false;
        btn_reverse.print(BLINKER_CMD_OFF);
    }
}

/**
 * 小米物联网查询回调
 * @param queryCode 查询码
 */
void miotQuery(int32_t queryCode) {
    switch (queryCode) {
        case BLINKER_CMD_QUERY_ALL_NUMBER :
            BLINKER_LOG("MIOT Query All");
            break;
        case BLINKER_CMD_QUERY_POWERSTATE_NUMBER :
            BLINKER_LOG("MIOT Query Power State");
            break;
        default:
            break;
    }
    BlinkerMIOT.powerState(lamp_state ? BLINKER_CMD_ON : BLINKER_CMD_OFF);
    BlinkerMIOT.print();
}

/**
 * 小米物联网控制回调
 * @param state 欲将设置的状态 on or off
 */
void miotPowerState(const String &state) {
    BLINKER_LOG("need set power state: ", state);
    if (state == BLINKER_CMD_ON) {
        BlinkerMIOT.powerState(BLINKER_CMD_ON);
        BlinkerMIOT.print();
        open_entity_lamp();
        showText("open by MIOT");
    } else if (state == BLINKER_CMD_OFF) {
        BlinkerMIOT.powerState(BLINKER_CMD_OFF);
        BlinkerMIOT.print();
        close_entity_lamp();
        showText("close by MIOT");
    } else {
        showText(state.c_str());
    }
}

/**
 * 关闭显示屏回调
 */
void screen_callback(const String &state) {
    oScreen = !oScreen;
    if (!oScreen)
        close_screen();
}



/**
 * 未捕获/绑定的数据
 * @param data json数据
 */
void dataRead(const String &data) {
    uint32_t BlinkerTime = millis();
    Blinker.print(BlinkerTime);
    Blinker.print("millis", BlinkerTime);

    text_reporter.print(lamp_state ? "灯开着" : "灯关着");
    BLINKER_LOG("Blinker readString: ", data);
    showText(data.c_str());

    int angle = strtol(data.c_str(), nullptr, 10);
    set_servo_A_angle(angle, 500);
    set_servo_B_angle(angle, 500);
}

/***********************************************base_work***********************************************/

/**
 * 工作循环
 */
void loop() {
    Blinker.run();
}

/**
 * 启动设置
 */
void setup() {
    Serial.begin(115200); //串口数据
    pinMode(LED_BUILTIN, OUTPUT);
    u8g2.begin();               //显示屏初始化
    showText("setup...");


    WiFiManager wifiManager;
    wifiManager.autoConnect();

    showText((WiFi.SSID() + ":" + WiFi.psk()).c_str());
    delay(1000);
    BLINKER_DEBUG.stream(Serial);
    BLINKER_DEBUG.debugAll();

    Blinker.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str());

    Blinker.attachData(dataRead);
    BlinkerMIOT.attachQuery(miotQuery);
    BlinkerMIOT.attachPowerState(miotPowerState);

    btn_open_lamp.attach(open_lamp_callback);
    btn_close_lamp.attach(close_lamp_callback);
    btn_open_led.attach(open_led_callback);
    btn_close_led.attach(close_led_callback);


    btn_switch.attach(btn_switch_callback);
    btn_plus.attach(plus_callback);
    btn_reverse.attach(reverse_callback);
    btn_checker.attach(checker_callback);
    btn_close_screen.attach(screen_callback);
    showText("setup done");

}
