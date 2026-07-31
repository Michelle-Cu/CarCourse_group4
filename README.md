# CarCourse_group4

## Results

Finger movements:
Robotic finger bends according to the flex sensors on the glove.
![Finger movements](/Results/hand_gestures.gif)

Punch:
Robotic arm ejects when the sensor glove's acceleration exceeds 1.5g.
![Punch](/Results/punch.gif)

Demo Video:
https://www.youtube.com/watch?v=hl6YXo5eNV0

## Project structure:

```mermaid
flowchart TB
    %% ==================== 手套端 (GLOVE SIDE) ====================
    subgraph GLOVE ["手套端感測 (Glove Controller)"]
        direction TB
        A["使用者手部動作"] --> B1["自製彎曲感測器 ×5<br/>(手指彎曲)"]
        A --> B2["MPU6050 加速度感測器<br/>(手腕姿態與速度)"]

        subgraph Flex_Proc ["手指訊號處理"]
            C1["讀取類比電壓值"] --> D1["Kalman Filter"]
            D1 --> E1["三段式角度校正"]
            E1 --> F1["映射至機器手指角度<br/>(0° ~ 180°)"]
        end

        subgraph Acc_Proc ["寸拳判定邏輯"]
            C2["讀取三軸加速度"] --> D2["計算運動加速度<br/>√(x² + y² + z²)"]
            D2 --> E2{"加速度<br/>> 1.5g (15 m/s²)?"}
            E2 -- 是 --> F2_1["設置出拳旗標 (Punch Flag = 1)"]
            E2 -- 否 --> F2_2["設置出拳旗標 (Punch Flag = 0)"]
        end

        B1 --> C1
        B2 --> C2

        F1 --> G1["資料整合與封包打包<br/>(PACKED Struct)"]
        F2_1 --> G1
        F2_2 --> G1
    end

    %% ==================== 通訊與接收 (COMMUNICATION) ====================
    G1 --->|ESP-NOW 無線通訊<br/> 6-Byte packet| H1["ESP32 接收端"]

    %% ==================== 接收端主控邏輯 (MAIN LOGIC) ====================
    subgraph MAIN_SYSTEM ["機械手臂"]
	    H1
    end

    %% ==================== 三維輸出端 (ROBOTIC HAND / ARM / AUDIO) ====================
    subgraph PALM_SYS ["手掌端 (Robotic Hand)"]
        H1 ----> K1["伺服馬達角度映射<br/>(硬體極限安全調校)"]
        K1 --> L1["寫入 5× Servo Motors"]
        L1 --> M1["驅動機械手指仿生運動"]
    end

    subgraph ARM_SYS ["手臂寸拳端 (Robotic Arm)"]
        H1 ----> K2{"出拳 flag == 1<br/>且 未處於出拳狀態<br/>且 冷卻時間 > 1秒?"}
        K2 -- 是 --> L2_1["點亮指示 LED &<br/>驅動步進馬達正轉 90°<br/>【寸拳強力彈出】"]
        K2 -- 否 --> L2_2["步進馬達保持靜態阻尼"]
        
        L2_1 --> M2_1["手臂恢復 <br/>(延遲 1 秒)"]
        M2_1 --> M2_2["驅動步進馬達正轉 270°"]
        M2_2 --> M2_3["關閉 LED &<br/>重啟 1秒冷卻計時器"]
        M2_3 --> L2_2
        
        L2_1 -.->|中斷驅動| N2["Hardware Timer ISR<br/>(高精密運動規劃)"]
        M2_2 -.->|中斷驅動| N2
        N2 --> O2["驅動 Stepper Motor"]
    end

    subgraph AUDIO_SYS ["音效端 (Audio System)"]
        H1 ----> K3["寸拳與動作動態音效觸發"]
        K3 --> L3["DFPlayer Mini 模組"]
        L3 --> M3["喇叭即時音效輸出"]
    end

    %% ==================== 視覺樣式設定 (THEMING) ====================
    style GLOVE fill:#e1f5fe,stroke:#03a9f4,stroke-width:2px,color:#000;
    style MAIN_SYSTEM fill:#fff9c4,stroke:#fbc02d,stroke-width:2px,color:#000;
    style PALM_SYS fill:#e8f5e9,stroke:#4caf50,stroke-width:2px,color:#000;
    style ARM_SYS fill:#ffe0b2,stroke:#fb8c00,stroke-width:2px,color:#000;
    style AUDIO_SYS fill:#f3e5f5,stroke:#9c27b0,stroke-width:2px,color:#000;
    
    style E2 fill:#ffe0b2,stroke:#fb8c00,stroke-width:2px,color:#000;
    style K2 fill:#ffe0b2,stroke:#fb8c00,stroke-width:2px,color:#000;
    style H1 fill:#3f51b5,stroke:#303f9f,color:#fff;
    style G1 fill:#03a9f4,stroke:#0288d1,color:#fff;
```
