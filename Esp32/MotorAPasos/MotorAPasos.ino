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
int currentStep = 0; // Posición actual en pasos desde el inicio (0-actualStepsPerRevolution)

// --- Variables de Control de Movimiento NO BLOQUEANTE ---
unsigned long lastStepTime = 0;
long stepsToMove = 0; // Número de pasos pendientes para movimientos específicos
bool continuousMode = true; // true para giro continuo con knob, false para movimientos por ángulo/pasos
// --------------------------------------------------------

const int FULL_STEPS_PER_REV = 2048; 
const int HALF_STEPS_PER_REV = 4096;
int actualStepsPerRevolution = HALF_STEPS_PER_REV;

bool halfStepMode = true; 
bool torqueEnabled = true; // Por defecto el torque está habilitado


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


void releaseTorque() {
    setMotorPins(0,0,0,0);
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

// --- Lógica para Oscilación ---
enum OscillationState {
    OSC_IDLE,
    OSC_RIGHT,
    OSC_STOP_RIGHT,
    OSC_LEFT,
    OSC_STOP_LEFT
};
OscillationState oscState = OSC_IDLE;
int oscCycles = 0;
const int MAX_OSC_CYCLES = 3; // Cuántas veces oscilar
const int OSC_ANGLE = 90; // Ángulo de oscilación
unsigned long oscStopTime = 0;
const unsigned long OSC_STOP_DELAY = 500; // ms para detenerse entre oscilaciones


void handleControl() {
    String command = server.arg("cmd");
    String value = server.arg("val");

    // Comentado para mejorar la estabilidad:
    // Serial.print("Comando recibido: ");
    // Serial.print(command);
    // Serial.print(", Valor: ");
    // Serial.println(value);

    // Definimos el header CORS. Lo enviaremos ANTES de cada server.send().
    const char* corsHeader = "Access-Control-Allow-Origin";
    const char* corsValue = "*";

    if (command == "enable") {
        motorEnabled = true;
        continuousMode = true; // Volver a modo continuo si se habilita
        if(torqueEnabled) { // Si el torque está habilitado, encendemos las bobinas
             executeOneStep();
        }
        server.sendHeader(corsHeader, corsValue); 
        server.send(200, "text/plain", "Motor Habilitado");
    } else if (command == "disable") {
        motorEnabled = false;
        stepsToMove = 0; // Detener cualquier movimiento específico pendiente
        continuousMode = true; 
        if(!torqueEnabled) { 
            releaseTorque();
        } 
        server.sendHeader(corsHeader, corsValue); 
        server.send(200, "text/plain", "Motor Deshabilitado");
    } else if (command == "direction") {
        // CORRECCIÓN 1: Usar motorSpeed para determinar si hay velocidad
        // motorSpeed < 4000 significa que el porcentaje de velocidad es > 0
        if (!motorEnabled && motorSpeed < 4000) { 
            server.sendHeader(corsHeader, corsValue);
            server.send(400, "text/plain", "Motor no habilitado para cambiar dirección de giro continuo.");
            return;
        }
        if (value == "left") { 
            motorDirectionCW = false;
            server.sendHeader(corsHeader, corsValue); 
            server.send(200, "text/plain", "Dirección: Izquierda (CCW)");
        } else if (value == "right") {
            motorDirectionCW = true;
            server.sendHeader(corsHeader, corsValue); 
            server.send(200, "text/plain", "Dirección: Derecha (CW)");
        } else if (value == "stop") { 
            stepsToMove = 0;
            motorEnabled = false; 
            if(!torqueEnabled) releaseTorque(); 
            server.sendHeader(corsHeader, corsValue); 
            server.send(200, "text/plain", "Giro continuo Detenido");
        }
        else {
            server.sendHeader(corsHeader, corsValue); 
            server.send(400, "text/plain", "Dirección inválida");
        }
    } else if (command == "speed") {
        int speedPercentage = value.toInt();
        // Mapeo inverso: 0% (lento, 4000us) a 100% (rápido, 1000us)
        motorSpeed = map(speedPercentage, 0, 100, 4000, 1000); 
        
        if (speedPercentage == 0) {
            stepsToMove = 0; 
            if(!torqueEnabled) releaseTorque(); 
            motorEnabled = false; 
            continuousMode = true; 
        } else {
            motorEnabled = true; 
            continuousMode = true; 
        }
        server.sendHeader(corsHeader, corsValue); 
        server.send(200, "text/plain", "Velocidad establecida: " + String(speedPercentage) + "%");
    } else if (command == "step_mode") {
        setSteppingMode(value == "half");
        server.sendHeader(corsHeader, corsValue); 
        server.send(200, "text/plain", "Modo de pasos establecido a: " + value);
    } 
    else if (command == "rotate_angle") {
        int angle = value.toInt();
        if (angle < 0 || angle > 360) {
            server.sendHeader(corsHeader, corsValue);
            server.send(400, "text/plain", "Ángulo inválido (0-360)");
            return;
        }
        
        float stepsPerDegree = (float)actualStepsPerRevolution / 360.0;
        stepsToMove = round(angle * stepsPerDegree);

        motorDirectionCW = true; 

        continuousMode = false; 
        motorEnabled = true; 
        Serial.printf("Girando %d grados, %ld pasos\n", angle, stepsToMove);
        server.sendHeader(corsHeader, corsValue);
        server.send(200, "text/plain", "Girando " + String(angle) + " grados.");
    }
    else if (command == "rotate_steps") {
        // CORRECCIÓN 2: Usar atol() para convertir String a long
        long numSteps = atol(value.c_str());
        
        stepsToMove = abs(numSteps); 
        motorDirectionCW = (numSteps >= 0); 
        
        continuousMode = false; 
        motorEnabled = true; 
        Serial.printf("Girando %ld pasos. Dirección CW: %d\n", stepsToMove, motorDirectionCW);
        server.sendHeader(corsHeader, corsValue);
        server.send(200, "text/plain", "Girando " + String(numSteps) + " pasos.");
    }
    else if (command == "enable_torque") {
        torqueEnabled = true;
        if(!motorEnabled) executeOneStep(); 
        server.sendHeader(corsHeader, corsValue);
        server.send(200, "text/plain", "Torque Habilitado.");
    }
    else if (command == "disable_torque") {
        torqueEnabled = false;
        if(!motorEnabled) releaseTorque(); 
        server.sendHeader(corsHeader, corsValue);
        server.send(200, "text/plain", "Torque Deshabilitado.");
    }
    else if (command == "oscillate") {
        oscState = OSC_RIGHT; 
        oscCycles = 0;
        continuousMode = false; 
        motorEnabled = true; 
        Serial.println("Iniciando oscilación.");
        server.sendHeader(corsHeader, corsValue);
        server.send(200, "text/plain", "Iniciando oscilación.");
    }
    else if (command == "return_to_zero") {
        int targetStep = 0;
        long stepsToGo = targetStep - currentStep;
        
        if (stepsToGo == 0) {
            server.sendHeader(corsHeader, corsValue);
            server.send(200, "text/plain", "Ya en posición cero.");
            return;
        }

        stepsToMove = abs(stepsToGo);
        motorDirectionCW = (stepsToGo > 0); 
        
        continuousMode = false; 
        motorEnabled = true; 
        Serial.printf("Volviendo a cero. Pasos: %ld. Dirección CW: %d\n", stepsToMove, motorDirectionCW);
        server.sendHeader(corsHeader, corsValue);
        server.send(200, "text/plain", "Volviendo a posición cero.");
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
    
    json += "\"speed\":" + String(map(motorSpeed, 4000, 1000, 0, 100)) + ","; 
    
    json += "\"stepMode\":\"";
    json += (halfStepMode ? "half" : "full");
    json += "\",";

    json += "\"torqueEnabled\":";
    json += (torqueEnabled ? "true" : "false");
    json += ",";

    json += "\"continuousMode\":";
    json += (continuousMode ? "true" : "false");
    
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

    releaseTorque(); 

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
    // 1. Maneja las peticiones web
    server.handleClient();
    
    // 2. Controla el motor de manera no bloqueante
    if (motorEnabled) {
        unsigned long currentMicros = micros(); 

        if (currentMicros - lastStepTime >= motorSpeed) {
            // Ejecutar paso SOLO si estamos en modo continuo O si quedan pasos pendientes
            if (continuousMode || stepsToMove > 0 || oscState != OSC_IDLE) {
                lastStepTime = currentMicros; 
                executeOneStep();
            }
            

            if (!continuousMode) { // Lógica para movimientos específicos
                if (stepsToMove > 0) {
                    stepsToMove--;

                    if (stepsToMove == 0) {
                        // Movimiento por pasos completado
                        Serial.println("Movimiento por pasos terminado.");
                        
                        // Manejar el final de un tramo de oscilación
                        if (oscState != OSC_IDLE) {
                            oscStopTime = millis(); 
                            if (oscState == OSC_RIGHT) oscState = OSC_STOP_RIGHT;
                            else if (oscState == OSC_LEFT) oscState = OSC_STOP_LEFT;
                        } else {
                            // Fin de un movimiento único (rotate_angle, return_to_zero)
                            motorEnabled = false; 
                            if (!torqueEnabled) releaseTorque(); 
                        }
                    }
                } else if (oscState != OSC_IDLE) { // Lógica de oscilación activa pero en pausa
                    if (millis() - oscStopTime >= OSC_STOP_DELAY) {
                        // Continuar oscilación
                        long desiredSteps = round(OSC_ANGLE * ((float)actualStepsPerRevolution / 360.0));

                        if (oscCycles < MAX_OSC_CYCLES) {
                            stepsToMove = desiredSteps;
                            motorDirectionCW = (oscState == OSC_STOP_LEFT); 
                           
                            if (oscState == OSC_STOP_RIGHT) oscState = OSC_LEFT;
                            else if (oscState == OSC_STOP_LEFT) oscState = OSC_RIGHT;
                           
                            motorEnabled = true; 
                            Serial.printf("Continuando oscilación, ciclo %d, hacia %s\n", oscCycles, motorDirectionCW ? "Derecha" : "Izquierda");
                            oscCycles++; // Incrementar el ciclo DESPUÉS de iniciar el movimiento
                        } else {
                            oscState = OSC_IDLE; 
                            motorEnabled = false; 
                            if (!torqueEnabled) releaseTorque();
                            Serial.println("Oscilación finalizada.");
                        }
                    }
                }
            } 
        }
    }
}