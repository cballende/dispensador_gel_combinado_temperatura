// ---------------------------------------------------------------------------
// Calculate a ping median using the ping_timer() method.
// ---------------------------------------------------------------------------
#include <Wire.h>
#include <NewPing.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>

#define grafico
//#define texto                   // util para debugger   
//Puedes cambiar estos valores
#define ITERATIONS      5       // Number of iterations.
#define MAX_DISTANCE    20      // Maximum distance (in cm) to ping.
#define timeOn          1200    //tiempo deseado de bombeo en milisegundos
#define manoNoDetectada 16      //distancia en cm
#define manoDetectada   14      //distancia en cm
#define tiempoEspera    1000
#define tiempoError     100
#define numPruebas      3

//Estos no
#define TRIGGER_PIN     2       // Arduino pin tied to trigger pin on ping sensor.
#define ECHO_PIN        4       // Arduino pin tied to echo pin on ping sensor.
#define PING_INTERVAL   33      // Milliseconds between sensor pings (29ms is about the min to avoid cross-sensor echo).

/************define de Temperatura********************/
//-----------------------------------------------------------
//Puedes cambiar estos valores
#define tempAlta         29.5   //Presencia de humano
#define tempOk           36     //Temperatura Ok a mostrar
#define tempRandom       36     // Temp Aleatoria
#define tempAlarma       37.5   //Temperatura de alarma
#define tiempoOn         2000   // Tiempo para apagar display
//#define factorTemp 1.17       //Factor de calibracion de temp
#define mediciones       3      //Cantidad de medidas por MEDICION
#define mediciones_check 10     //Cantidad de MEDICIONES del ciclo
#define Emisividad       0.6     //Cantidad de MEDICIONES del ciclo

//pines
#define ledVerde         10
#define ledRojo          11
#define buzzer           12
#define sensor           11

/******TEXTO de control DISPLAY de temperatura ****/
/******TEXTO de control DISPLAY de temperatura ****/
/******TEXTO de control DISPLAY de temperatura ****/
/*Texto de standby 1*/
const String texto1= "  Controla Tu  ";
const String texto2= "  TEMPERATURA  ";
/*Texto de standby 2*/
const String texto3= "T Amb.:";
const String texto4= " ^C";
/*Texto de Pront */
const String texto5= "AGUARDE";
const String texto6= "PROCESESANDO";
/*Texto de resultado medicion */
const String texto7= "Su Temperatura:";
const String texto8= " ^C";




/******Variables de control de TEMPERATURA ****/

float temp= 36;
bool  screenStatus= false;
int   conteoTemp=0;
//float randomNumber;
//float randomNumber2;
//bool  st =false;
float mesures[10]; 
byte  j=0;

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
LiquidCrystal_I2C lcd(0x27,16,2);


/****** TEXTO de control SONAR ****************/

unsigned long pingTimer[ITERATIONS]; // Holds the times when the next ping should happen for each iteration.
unsigned int  cm[ITERATIONS];         // Where the ping distances are stored.
uint8_t       currentIteration = 0;        // Keeps track of iteration step.
bool          manoStatus=false;
const int     pumpControl = 5; //Pin de salida para el relay/driver de la bomba
const int     pumpControl2 = 6; //Pin de salida 2 para el relay/driver de la bomba
unsigned long timeNow; //variable interna para conteo de tiempo
int           iteracionesDeteccion=0;


NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.

void setup() {
  /*******Para debugger**********/
  /*******Para debugger**********/
  Serial.begin(115200);
  
  /*****************Setup de Aplicacion TEMPERATURA *********************/
  /*****************Setup de Aplicacion TEMPERATURA *********************/
  pinMode(sensor, OUTPUT);
  digitalWrite(sensor,LOW);
  
  mlx.begin();
  /*controla el sesgo de la medicion de temperatura*/
  mlx.writeEmissivity(Emisividad);
  
  lcd.init();
  lcd.backlight();
  pantallaStandby();
  //pinMode(ledVerde, OUTPUT);
  //pinMode(ledRojo, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer,HIGH);   
  //pinMode(LED_BUILTIN, OUTPUT);
  
  /******************setup de aplicacion SONAR *************************/
  /******************setup de aplicacion SONAR *************************/
  
  pinMode(pumpControl, OUTPUT);//se establece pin del control de bomba como salida
  pinMode(pumpControl2, OUTPUT);//se establece pin 2 del control de bomba como salida
  digitalWrite(pumpControl, LOW);//Se lleva a tierra dicho pin
  digitalWrite(pumpControl2, LOW);//Se lleva a tierra dicho pin
  pingTimer[0] = millis() + 75;            // First ping starts at 75ms, gives time for the Arduino to chill before starting.
  for (uint8_t i = 1; i < ITERATIONS; i++) // Set the starting time for each iteration.
    pingTimer[i] = pingTimer[i - 1] + PING_INTERVAL;
}

/***********************************************************************/
/************** FUNCION PRINCIPAL   ************************************/
/***********************************************************************/

void loop() {

  if (Sonar_Fun())
  {
   while (Temp_Fun());
   digitalWrite(pumpControl,HIGH);// activa la bomba   
   delay(timeOn);
   digitalWrite(pumpControl,LOW);
   delay(tiempoEspera);
   pantallaStandby();   
  }
  
  // Other code that *DOESN'T* analyze ping results can go here.
} //EndLOOP


/***********************************************************************/
/************** FUNCIONES QUE ASISTEN AL SONAR *************************/
/***********************************************************************/

bool Sonar_Fun(void){
  bool st=false;
  
  for (uint8_t i = 0; i < ITERATIONS; i++) 
  { // Loop through all the iterations.
    if (millis() >= pingTimer[i])  // Is it this iteration's time to ping?
    {          
      pingTimer[i] += PING_INTERVAL * ITERATIONS; // Set next time this sensor will be pinged.
      if (i == 0 && (currentIteration == (ITERATIONS - 1))) 
        if (oneSensorCycle())      // Sensor ping cycle complete, do something with the results.        
          st=true;       
      sonar.timer_stop();          // Make sure previous timer is canceled before starting a new ping (insurance).
      currentIteration = i;        // Sensor being accessed.
      cm[currentIteration] = 0;    // Make distance zero in case there's no ping echo for this iteration.
      sonar.ping_timer(echoCheck); // Do the ping (processing continues, interrupt will call echoCheck to look for echo).
    }
  }
  return(st);
}

void echoCheck() { // If ping received, set the sensor distance to array.
  if (sonar.check_timer())
    cm[currentIteration] = sonar.ping_result / US_ROUNDTRIP_CM;
}

bool oneSensorCycle() { // All iterations complete, calculate the median.
  unsigned int uS[ITERATIONS];
  uint8_t j, it = ITERATIONS;
  uS[0] = NO_ECHO;
  for (uint8_t i = 0; i < it; i++) 
  { // Loop through iteration results.
    if (cm[i] != NO_ECHO) 
    { // Ping in range, include as part of median.
      if (i > 0) 
      {          // Don't start sort till second ping.
        for (j = i; j > 0 && uS[j - 1] < cm[i]; j--) // Insertion sort loop.
          uS[j] = uS[j - 1];                         // Shift ping array to correct position for sort insertion.
      } else
        j = 0;         // First ping is sort starting point.
      uS[j] = cm[i];        // Add last ping to array in sorted position.
    }else
      it--;            // Ping out of range, skip and don't include as part of median.
  }
  #ifdef grafico
  if( (uS[it >> 1] <= 20) && (uS[it >> 1]>0) )
  {
    Serial.println(uS[it >> 1]);
  }
  #endif
  if(uS[it >> 1] >=manoNoDetectada)
  {
    iteracionesDeteccion=0;
    if(manoStatus==true)
    {
      #ifdef texto
        //Serial.println("Mano no detectada");
      #endif
        //digitalWrite(pumpControl, LOW); //Apaga la bomba
        manoStatus=false;        
    }
    
  }
  else if(uS[it >> 1] <=manoDetectada)
  {
    if(manoStatus==false)
    {
      iteracionesDeteccion=iteracionesDeteccion+1;
      #ifdef texto
      Serial.print("Mano Detectada, iteracion ");Serial.println(iteracionesDeteccion);
      #endif
      if(iteracionesDeteccion==numPruebas)
      {
        
        iteracionesDeteccion=0;
        #ifdef texto
        Serial.println("Bomba activada");
        Serial.println("");
        #endif
        //digitalWrite(pumpControl,HIGH);// activa la bomba
        manoStatus=true;
        //delay(timeOn);
        //digitalWrite(pumpControl,LOW);
        //delay(tiempoEspera);  
        return(true);      
      }
    }    
  }
  return(false);
}


/***********************************************************************/
/************** FUNCIONES QUE ASISTEN A SENSOR TEMPERATURA**************/
/***********************************************************************/

bool Temp_Fun(void){

  static bool St= false;

  switch (St)
  {
    case false:
          temp = 0;
          //for(int i=0;i<mediciones;i++)
          //{
          //  delay(50);
          //  temp += mlx.readObjectTempC();    
          //}  
          //temp /= mediciones;//*factorTemp;
          
          //Serial.println(temp);
          //Serial.println("°C");
          //digitalWrite(LED_BUILTIN,LOW); 
        
          //if((temp >= tempAlta) &&  conteoTemp)
          //{    
            conteoTemp= false;
            //if(screenStatus == false){
            //  screenStatus= true;
             // digitalWrite(LED_BUILTIN,HIGH); 
            //}
            //digitalWrite(LED_BUILTIN,HIGH); 
            //if(temp <= tempOk)
            //{
            //  randomNumber=random(0,7)*0.1;
            //  randomNumber2=random(0,10)*0.01; 
            //  temp = tempRandom+randomNumber+randomNumber2;
            //}
            //Serial.print(temp); Serial.println(" ^C");
            
            //pantallaPront();
            for(int i=0;i<mediciones_check;i++)
            {
              delay(10);                
              temp = 0;
              for(int i=0;i<mediciones;i++)
              {
                delay(10);
                temp += mlx.readObjectTempC();    
              }  
              mesures[i]=temp/mediciones;//*factorTemp;  
              #ifdef grafico    
              Serial.print(mesures[i]); Serial.println(" ^C");
              #endif
            }
            
            j=0;
            for(int i=0;i<mediciones_check;i++)
            {
              if (mesures[i]>=tempAlarma){     
                 temp= mesures[i];
                j++;      
              }  
            }
            
            if ( j > (mediciones_check/2))
            {
                //temp= tempAlarma+ random(0,5)*0.1;    
                pantallaTemp();
                //digitalWrite(ledRojo, HIGH);         
                alarm();
                
            }else
            {
              for(int i=0;i<mediciones_check;i++)
              {
                temp=36+ random(0,7)*0.1;
                if ((mesures[i]<tempAlarma) &&(mesures[i]>temp))
                {
                  temp=mesures[i];
                }      
              }              
              pantallaTemp();  
              //digitalWrite(ledVerde, HIGH);
              digitalWrite(buzzer,LOW);
              delay(250);
              digitalWrite(buzzer, HIGH);
            }    
            //temp /= mediciones;//*factorTemp;
            
            //delay(tiempoOn);
            //digitalWrite(ledRojo, LOW);
            //digitalWrite(ledVerde, LOW);
            St= true;
            return(true);
    break;
    
    case true:
            //pantallaStandby(); 
            St= false;
            //screenSt == false;
            
            return (false);
            
    break;
  }  
  
    
  //}else if(screenStatus == true)
  //  {   
      //digitalWrite(LED_BUILTIN,HIGH); 
      
      //delay(2000);              
  //  }
  
  //conteoTemp= true;
  
  //delay(1000);     
  
}//EndTemp_fun


void pantallaStandby(void){
  
      conteoTemp=0;
      screenStatus= false;
      //lcd.noBacklight();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(texto1);
      lcd.setCursor(0, 1);
      lcd.print(texto2);

}

void pantallaStandby1(void){
  
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(texto3);
      lcd.setCursor(0, 1);
      lcd.print(temp);    
      lcd.print(texto4);
      
}

void pantallaPront(void){
  
      conteoTemp=0;
      //screenStatus=false;
      //lcd.noBacklight();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(texto5);
      lcd.setCursor(0, 1);
      lcd.print(texto6);

}

void pantallaTemp(void){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(texto7);
  lcd.setCursor(4,1);
  lcd.print(temp);
  lcd.print(texto8);
}

void alarm(void){
    
    digitalWrite(buzzer, LOW);
    delay(250);
    digitalWrite(buzzer,HIGH);
    delay(150);
    digitalWrite(buzzer, LOW);
    delay(250);
    digitalWrite(buzzer,HIGH);
    delay(150);
    digitalWrite(buzzer, LOW);
    delay(250);
    digitalWrite(buzzer,HIGH);
    delay(150);
    digitalWrite(buzzer, LOW);
    delay(250);
    digitalWrite(buzzer,HIGH);
}
