#include <WiFi.h>
#include <WebServer.h>

// Configuración de WiFi
const char* ssid = "TU_WIFI_SSID";
const char* password = "TU_WIFI_PASSWORD";

WebServer server(80);

// Pines del ESP32 para el driver ULN2003
const int IN1 = 23;  // GPIO23
const int IN2 = 22;  // GPIO22
const int IN3 = 21;  // GPIO21
const int IN4 = 19;  // GPIO19

// Variables del motor
int motorSpeed = 1000; //menor valor = mayor velocidad
bool motorEnabled = false;
bool motorDirectionCW = true; // true para Derecha, false Izquierda
int currentStep = 0; 


const int FULL_STEPS_PER_REV = 2048; 
const int HALF_STEPS_PER_REV = 4096;
int actualStepsPerRevolution = HALF_STEPS_PER_REV; 

bool halfStepMode = true; 


const int fullStepSequence[4][4] = {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1}
};

const int halfStepSequence[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

int stepIndex = 0; 


void setMotorPins(int s1, int s2, int s3, int s4) {
    digitalWrite(IN1, s1);
    digitalWrite(IN2, s2);
    digitalWrite(IN3, s3);
    digitalWrite(IN4, s4);
}


void setSteppingMode(bool isHalfStep) {
    halfStepMode = isHalfStep;
    if (halfStepMode) {
        actualStepsPerRevolution = HALF_STEPS_PER_REV;
        Serial.println("Modo: Medios Pasos");
    } else {
        actualStepsPerRevolution = FULL_STEPS_PER_REV;
        Serial.println("Modo: Pasos Completos");
    }
}

void moveMotorSteps(int stepsToMove, bool directionCW) {
    if (!motorEnabled) return;

    int totalSteps = abs(stepsToMove);
    int sequenceLength = halfStepMode ? 8 : 4;

    for (int i = 0; i < totalSteps; i++) {
        if (directionCW) { 
            stepIndex = (stepIndex + 1) % sequenceLength;
            currentStep = (currentStep + 1) % actualStepsPerRevolution;
        } else {
            stepIndex = (stepIndex - 1 + sequenceLength) % sequenceLength;
            currentStep = (currentStep - 1 + actualStepsPerRevolution) % actualStepsPerRevolution;
        }

        if (halfStepMode) {
            setMotorPins(halfStepSequence[stepIndex][0], halfStepSequence[stepIndex][1], halfStepSequence[stepIndex][2], halfStepSequence[stepIndex][3]);
        } else {
            setMotorPins(fullStepSequence[stepIndex][0], fullStepSequence[stepIndex][1], fullStepSequence[stepIndex][2], fullStepSequence[stepIndex][3]);
        }
        delayMicroseconds(motorSpeed);
    }
}

void handleControl() {
    String command = server.arg("cmd");
    String value = server.arg("val");

    Serial.print("Comando recibido: ");
    Serial.print(command);
    Serial.print(", Valor: ");
    Serial.println(value);

    if (command == "enable") {
        motorEnabled = true;
        server.send(200, "text/plain", "Motor Habilitado");
    } else if (command == "disable") {
        motorEnabled = false;
        setMotorPins(0, 0, 0, 0);
        server.send(200, "text/plain", "Motor Deshabilitado");
    } else if (command == "direction") {
        if (value == "left") { 
            motorDirectionCW = false;
            server.send(200, "text/plain", "Dirección: Izquierda (CCW)");
        } else if (value == "right") {
            motorDirectionCW = true;
            server.send(200, "text/plain", "Dirección: Derecha (CW)");
        } else if (value == "stop") {
            setMotorPins(0,0,0,0); 
            motorEnabled = false; 
            server.send(200, "text/plain", "Motor Detenido");
        }
        else {
            server.send(400, "text/plain", "Dirección inválida");
        }
    } else if (command == "speed") {
        int speedPercentage = value.toInt();
        motorSpeed = map(speedPercentage, 0, 100, 4000, 1000);
        server.send(200, "text/plain", "Velocidad establecida: " + String(speedPercentage) + "%");
    } else if (command == "step_mode") {
        setSteppingMode(value == "half");
        server.send(200, "text/plain", "Modo de pasos establecido a: " + value);
    } else if (command == "rotate_angle") {
        if (motorEnabled) {
            float angle = value.toFloat();
            int stepsToRotate = (int)(angle / 360.0 * actualStepsPerRevolution);
            Serial.print("Girando ");
            Serial.print(stepsToRotate);
            Serial.println(" pasos.");
            moveMotorSteps(stepsToRotate, motorDirectionCW);
            server.send(200, "text/plain", "Girando " + String(angle) + " grados");
        } else {
            server.send(400, "text/plain", "Motor deshabilitado. Enciende el motor primero.");
        }
    }
    else {
        server.send(400, "text/plain", "Comando inválido");
    }
}


void handleStatus() {
    String json = "{";
    json += "\"currentStep\":" + String(currentStep) + ",";
    json += "\"motorEnabled\":" + (motorEnabled ? "true" : "false") + ",";
    json += "\"direction\":\"" + (motorDirectionCW ? "right" : "left") + "\",";
    json += "\"speed\":" + String(motorSpeed) + ",";
    json += "\"stepMode\":\"" + (halfStepMode ? "half" : "full") + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);

    setMotorPins(0, 0, 0, 0);

    setSteppingMode(true);

    Serial.print("Conectando a ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi conectado.");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());

    server.on("/control", HTTP_GET, handleControl);
    server.on("/status", HTTP_GET, handleStatus);
    server.begin();
    Serial.println("Servidor HTTP iniciado.");
}

void loop() {
    server.handleClient();
}