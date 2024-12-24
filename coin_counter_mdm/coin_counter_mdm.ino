 
/**
 * ModuleMore
 * 
 * ตัวอย่างการใช้งานตัวอ่านเหรียญ ด้วย ESP32
 * 
 * วิธีการต่อ 
 * | ESP32 | ตัวอ่านเหรียญ |
 * |--------|------------------|
 * | GND    | GND              |
 * | 33     | COIN             |
 * 
 * โดยตัวอ่านเหรียญจ่ายไฟเลี้ยงด้วย 12VDC มี Ground ร่วมกับ ESP32
 * 
 * ไม่ต้องใช้ไลบรารี่เพิ่มเติม ตัว class Coin สามารถย้ายไปยังไฟล์ .h ได้
 * 
 * รองรับการอ่านเหรียญ 1, 2, 5, 10 บาท และเหรียญ Burst ที่มีการหยอดเหรียญหลายๆเหรียญอย่างรวดเร็ว
 * 
 * @copyright Copyright (c) 2024
 * 
 */
 
#include <Arduino.h> 


#include <unordered_map>
#include "FunctionalInterrupt.h"

#define COIN_PIN 33

class Coin {
  public:
    typedef enum coin_t{
      COIN_BURST,
      COIN_1,
      COIN_2,
      COIN_5,
      COIN_10,
      COIN_N
    }coin_t;     
  private: 
    typedef enum state_t{
      ST_IDLE,
      ST_INSERTING,
      ST_INSERTED,
    }state_t; 
    int m_state = ST_IDLE;
    int m_coinAmount = 0; 
    int m_tripAmount = 0;
    unsigned long m_timestamp = 0;
    unsigned long m_timestamp_prev = 0;
    std::unordered_map<int, coin_t> m_valueToCoin;
    std::unordered_map<coin_t, int> m_coinToCount;
    std::unordered_map<coin_t, int> m_totalValue;
    int m_triggerCount = 0; 
    bool m_debug = true;
  public:
    void isr() {
      m_timestamp  = millis(); 
      if(m_timestamp - m_timestamp_prev > 70) {
        m_triggerCount++;
        m_timestamp_prev = m_timestamp;

      } 
    }
    void init( ) {
      Serial.println("Coin init"); 
      pinMode(COIN_PIN, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(COIN_PIN), std::bind(&Coin::isr, this), FALLING);

      m_valueToCoin[1] = COIN_1;
      m_valueToCoin[2] = COIN_2;
      m_valueToCoin[5] = COIN_5;
      m_valueToCoin[10] = COIN_10;
      
      reset();
    } 

    void debug(bool debug) {
      m_debug = debug;
    }
    
    void update() { 
      if(m_valueToCoin.empty()) { 
        Serial.println("Coin not initialized, call init() first");
        return;
      }
      if(m_state==ST_IDLE && m_triggerCount>0) {
        m_state = ST_INSERTING;

        if(m_debug) {
          Serial.println("------");
          Serial.println("Coin inserting");
        }
      } 

      if(m_state==ST_INSERTING) {  
        
        bool timeout = millis() - m_timestamp > 150;
        if(timeout) {
          m_coinAmount = m_triggerCount;
          m_timestamp = 0;
          m_state = ST_INSERTED; 
        }
      }

    }  
    bool inserted() {  
      if(m_state == ST_INSERTED) {
        coin_t coinType;
        if(m_coinAmount>10) {
          coinType = COIN_BURST;
          m_totalValue[COIN_BURST] += m_coinAmount;
        }else {
          coinType = m_valueToCoin[m_coinAmount];
          m_totalValue[coinType] += m_coinAmount;
        }

        m_coinToCount[coinType] ++;
        m_tripAmount+=m_coinAmount;
        m_state = ST_IDLE;
        m_triggerCount = 0;

        if(!m_debug) return true;

        Serial.print("Coin INSERTED: ");
        Serial.println(m_coinAmount);
        Serial.print(" Coin 1: ");
        Serial.println(m_coinToCount[COIN_1]);
        Serial.print(" Coin 2: ");
        Serial.println(m_coinToCount[COIN_2]);
        Serial.print(" Coin 5: ");
        Serial.println(m_coinToCount[COIN_5]);
        Serial.print(" Coin 10: ");
        Serial.println(m_coinToCount[COIN_10]);
        Serial.print(" Coin Burst: ");
        Serial.println(m_coinToCount[COIN_BURST]);
        Serial.print(" inserted total = ");
        Serial.println(total());
        Serial.println("------");
        return true;
      } 
      return false;
    }
    bool isInserting() {
      return m_state==ST_INSERTING;
    }
    
    int lastCoinAmount() {
      return m_coinAmount;
    }

    int getTripAmount() {
      return m_tripAmount;
    }

    void resetTrip() {
      m_tripAmount = 0;
    }

    void reset() { 
      for(auto coin:m_coinToCount) coin.second = 0;
      for(auto value:m_totalValue) value.second = 0;
      m_tripAmount = 0;
    }
    int total() {
      int total = 0;
      for(auto value:m_totalValue) 
        total+=value.second;
      return total;
    }
 
};


/* --------------------------------- Object --------------------------------- */
Coin _coin; // สร้าง object สำหรับเซ็นเซอร์เหรียญ 
int _targetAmount = 50;
/* ---------------------------------- Start --------------------------------- */
void setup() {  
  Serial.begin(115200);
  while (!Serial) { ;  }
  Serial.println("\n\n\nSerial setup at 115200"); 
  _coin.init(); 
  _coin.debug(false); // true คือแสดงปริมาณเหรียญที่หยอด และค่าที่หยอดออกทาง Serial Monitor
  
  Serial.println("Setup done");
}

void loop() {
  _coin.update();     // ตรวจสอบสัญญาณจากเซ็นเซอร์เหรียญ 

  if(_coin.inserted()) {
    Serial.println("Coin inserted");

    // เมื่อถึงมูลค่าที่ต้องการ ให้ทำอะไร
    if(_coin.getTripAmount()>=_targetAmount) {
      // DO SOMETHING
      Serial.println("WORKING ON SOMETHING");
      // รีเซ็ตค่า trip 
      _coin.resetTrip();
    }else{
      Serial.print(_targetAmount - _coin.getTripAmount());
      Serial.println(" To work.");
    }
  }

  if(_coin.isInserting()){
    // บอกให้ผู้ใช้ทราบว่ากำลังอ่านเหรียญ กรุณารอสักครู่
  } 
 
}  