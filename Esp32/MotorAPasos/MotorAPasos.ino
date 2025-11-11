#include <WiFi.h>
#include <WebServer.h>

// Configuración de WiFi
const char* ssid = "Fam_Flores-2024";
const char* password = "ChucheRoloBeto4488";

WebServer server(80);

// Pines del ESP32 para el driver ULN2003
const int IN1 = 23;  // GPIO23
const int IN2 = 22;  // GPIO22
const int IN3 = 21;  // GPIO21
const int IN4 = 19;  // GPIO19

// Variables del motor
int motorSpeed = 1000; // menor valor = mayor velocidad (us/paso)
bool motorEnabled = false;
bool motorDirectionCW = true; // true para Derecha (CW), false Izquierda (CCW)
int currentStep = 0; 

// --- Variables de Control de Movimiento NO BLOQUEANTE ---
unsigned long lastStepTime = 0;
// --------------------------------------------------------

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

/**
 * Función NO BLOQUEANTE para ejecutar un solo paso del motor.
 */
void executeOneStep() {
    int sequenceLength = halfStepMode ? 8 : 4;
    
    if (motorDirectionCW) { 
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
}

// =================================================================================
// NOTA: moveMotorSteps() ha sido ELIMINADA para evitar el bloqueo del ESP32.
// Ahora se usa una lógica de temporización en el loop().
// =================================================================================


void handleControl() {
    String command = server.arg("cmd");
    String value = server.arg("val");

    Serial.print("Comando recibido: ");
    Serial.print(command);
    Serial.print(", Valor: ");
    Serial.println(value);

    // Definimos el header CORS. Lo enviaremos ANTES de cada server.send().
    const char* corsHeader = "Access-Control-Allow-Origin";
    const char* corsValue = "*";

    if (command == "enable") {
        motorEnabled = true;
        server.sendHeader(corsHeader, corsValue); 
        server.send(200, "text/plain", "Motor Habilitado");
    } else if (command == "disable") {
        motorEnabled = false;
        setMotorPins(0, 0, 0, 0);
        server.sendHeader(corsHeader, corsValue); 
        server.send(200, "text/plain", "Motor Deshabilitado");
    } else if (command == "direction") {
        // En este modo, la dirección solo cambia el sentido del giro continuo
        if (value == "left") { 
            motorDirectionCW = false;
            server.sendHeader(corsHeader, corsValue); 
            server.send(200, "text/plain", "Dirección: Izquierda (CCW)");
        } else if (value == "right") {
            motorDirectionCW = true;
            server.sendHeader(corsHeader, corsValue); 
            server.send(200, "text/plain", "Dirección: Derecha (CW)");
        } else if (value == "stop") {
            setMotorPins(0,0,0,0); 
            motorEnabled = false; 
            server.sendHeader(corsHeader, corsValue); 
            server.send(200, "text/plain", "Motor Detenido");
        }
        else {
            server.sendHeader(corsHeader, corsValue); 
            server.send(400, "text/plain", "Dirección inválida");
        }
    } else if (command == "speed") {
        int speedPercentage = value.toInt();
        // Mapeo inverso: 0% (lento, 4000us) a 100% (rápido, 1000us)
        motorSpeed = map(speedPercentage, 0, 100, 4000, 1000); 
        motorEnabled = (motorSpeed != 4000); // Si la velocidad es 0%, el motor está detenido
        server.sendHeader(corsHeader, corsValue); 
        server.send(200, "text/plain", "Velocidad establecida: " + String(speedPercentage) + "%");
    } else if (command == "step_mode") {
        setSteppingMode(value == "half");
        server.sendHeader(corsHeader, corsValue); 
        server.send(200, "text/plain", "Modo de pasos establecido a: " + value);
    } 
    // NOTA: El comando "rotate_angle" se ha eliminado temporalmente.
    // Para implementar un giro por ángulo, se requeriría lógica adicional (contar pasos restantes)
    // que es más complejo que el simple giro continuo.
    else if (command == "rotate_angle") {
         server.sendHeader(corsHeader, corsValue); 
         server.send(400, "text/plain", "Comando 'rotate_angle' deshabilitado en modo continuo.");
    }
    else {
        server.sendHeader(corsHeader, corsValue); 
        server.send(400, "text/plain", "Comando inválido");
    }
}


void handleStatus() {
    String json = "{";
    json += "\"currentStep\":" + String(currentStep) + ",";
    
    json += "\"motorEnabled\":";
    json += (motorEnabled ? "true" : "false");
    json += ","; 
    
    json += "\"direction\":\"";
    json += (motorDirectionCW ? "right" : "left");
    json += "\",";
    
    json += "\"speed\":" + String(motorSpeed) + ",";
    
    json += "\"stepMode\":\"";
    json += (halfStepMode ? "half" : "full");
    json += "\"";
    
    json += "}";
    server.sendHeader("Access-Control-Allow-Origin", "*"); 
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
    // 1. Maneja las peticiones web (esto debe ser lo más rápido posible)
    server.handleClient();
    
    // 2. Controla el motor de manera no bloqueante
    if (motorEnabled) {
        unsigned long currentMicros = micros(); 

        // Comprueba si ha pasado el tiempo necesario (motorSpeed)
        if (currentMicros - lastStepTime >= motorSpeed) {
            lastStepTime = currentMicros; // Guarda el tiempo del paso actual
            
            // Ejecuta el movimiento de UN paso
            executeOneStep();
        }
    }
}