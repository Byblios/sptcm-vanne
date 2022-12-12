//////////////////////////////////////////////////////////////////////////////////  // gsm Local : +33630466723, Admin: Pierrot +33631881147 Ra +33681854641
//
// commande SMS   
//      #stat                   -> renvoie SMS stat au numero appelant
//      #purge                  -> confirme SMS aux numeros admin
//      #alim                   -> confirme SMS aux numeros admin
//      #auto ou #auto.270.280  -> confirme SMS aux numeros admin  //    force mode automatique, ou AUTO.kelvin_purge.kelvin_alim   exemple AUTO.272.276 par defaut kelvin_seuil_purge=272 kelvin_seuil_alim=278,
// info stat SMS  :
//  info temperature journaliére en degré kelvin par tranche de 4 heures :  Jour(t0,t1,t2,t3,t4,t5)
//
// mode AUTO
//   si la temperature descend en dessous de kelvin_seuil_purge pendant plus de delai_seuil_purge (10 minutes) -> maneuvre purge, si elle monte audessus de kelvin_seuil_alim pendant plus de delai_seuil_alim (une heure) -> maneuvre alim
//   la maneuvre de purge ou d'alim par #PURGE, #ALIM ou #AUTO doit finir avec la détection de fin de course, stat_vanne passe alors à VANN_ALIM_OK ou VANN_PURGE_OK
//   si la fin de course n'est pas détectée après TEMPO_VANNE_DECI_SEC, stat_vanne passe à VANN_ALIM_ERR ou VANN_PURGE_ERR le mode AUTO est inhibé, 
//                on doit renvoyer une commande #PURGE ou #ALIM et obtenir VANN_ALIM_OK ou VANN_PURGE_OK avant de repasser en mode #AUTO

#define MODE_AUTO  0  
#define MODE_PURGE 1  //    force mode purge
#define MODE_ALIM  2  //    force mode alim
int mode_vanne=MODE_AUTO;  // commande par input ou recept SMS 

#define DELAY_PURGE  6   // POUR TEST  #define DELAY_PURGE  10 
uint16_t delai_seuil_purge=0;
#define DELAY_ALIM   10   // POUR TEST  #define DELAY_ALIM   60
uint16_t delai_seuil_alim=0;

#define SEUIL_PURGE_STD 272
#define SEUIL_ALIM_STD 278
uint16_t kelvin_seuil_purge=SEUIL_PURGE_STD;
uint16_t kelvin_seuil_alim=SEUIL_ALIM_STD;
//****************************************************
#define VANN_INIT      0
#define VANN_PURGE_OK  1
#define VANN_PURGE_REQ 2 
#define VANN_PURGE_ERR 3
#define VANN_ALIM_OK   4
#define VANN_ALIM_REQ  5 
#define VANN_ALIM_ERR  6 
int statVanne = VANN_INIT;
int duree_manoeuvre = 0;

#define TEMPO_VANNE_DECI_SEC 52  // temp maximum de manoeuvre vanne, sinon VANN_PURGE_ERR ou VANN_ALIM_ERR -> mode M_AUTO suspendu

char szStatVann[][16]={"VANNE_INIT","VANNE_PURGE_OK","VANNE_PURGE_REQ","VANNE_PURGE_ERR","VANNE_ALIM_OK","VANNE_ALIM_REQ","VANNE_ALIM_ERR","?"};
char szModeVanne[][8]={"M_AUTO","M_PURGE","M_ALIM"};

#define FWD_RELAY_OUT  2
#define FWD_SWITCH_IN  10
#define REV_RELAY_OUT  3
#define REV_SWITCH_IN  11

//////////////////////////////////
#include <SoftwareSerial.h>

SoftwareSerial Sim900A(8,9); // RX, TX
#define MAX_REC 196
char szRec[MAX_REC]; 
int8_t cbRec;
//////////////////////////////////////////////////////////////////////////////////
//char szT[256];
//************************************************************
#include "iarduino_RTC.h"
iarduino_RTC Horloge(RTC_DS1302, 7, 5, 6); // pour module (DS1302, rst, clk, dat)   [ VCC GND CLK->5 DAT->6 RST->7 ]
//***************************************
uint64_t millis64() 
{ 
  static uint32_t precti=0;
  static uint64_t over_millis=0;
  uint32_t ti=millis();
    if (ti<precti) over_millis++;
    precti=ti;
   uint64_t res=(over_millis<<32)+ti;
    return res;
}
//***************************
uint32_t deci_seconde(){  return millis64()/100;}  // overflow ~5000 jours
//************************************************************
//int cptsec = 0, cptmin = 0, cptheur = 0, cptjour = 0;
void cpttime(){
  static uint32_t secmem=0;
  uint32_t secnow=deci_seconde();
  if(secnow>=secmem+600) 
  {
    secmem=secnow;
    process_minute();
    //Serial.write("Cpt minute "); Serial.println(secnow);
  }
/*  static uint32_t last_sec;
  uint32_t sec = millis64() / 1000;
  if (last_sec == sec) return;

  cptsec+=sec-last_sec;
  last_sec=sec;
    //Serial.write("tic  ");
  while (cptsec > 59) {    cptsec  -= 60;    cptmin++;    process_minute();  }  
  while (cptmin > 59) {    cptmin  -= 60;    cptheur++;  }// process_heure();   } // comptage H m s prévu si pas de RealTimeClock
  while (cptheur > 23){    cptheur -= 24;    cptjour++;  }*/
}
//*************************************************************
//*************************************************************
void setup() {
  pinMode(FWD_RELAY_OUT, OUTPUT);   digitalWrite(FWD_RELAY_OUT, HIGH);
  pinMode(FWD_SWITCH_IN, INPUT);

  pinMode(REV_RELAY_OUT, OUTPUT);   digitalWrite(REV_RELAY_OUT, HIGH);
  pinMode(REV_SWITCH_IN, INPUT);
  
  szRec[0]=0;

  Serial.begin(9600);
  Sim900A.begin(38400);// 19200 38400 115200 
  delay(400);

  //Sim900A.println("AT+IPR=38400");   delay(200);
  
  Serial.write("** AUTO ALIM PURGE *** DateCompil:");Serial.write(__DATE__);Serial.println(" ******");
  Serial.println("**** INIT SIM900-> AT+CNMI=2,2,0,0,0 rec SMS direct");

  Sim900A.println("AT+CMGF=1");   delay(500);  // GSM  in Text Mode
  Sim900A.println("AT+CNMI=2,2,0,0,0"); delay(500);  //Sim900A.println("AT+CNMI=2,2,0,0,0");
  Sim900A.println("AT+CMGF=1");   delay(500);  // GSM  in Text Mode
 //+CSQ service quality report / +GSV ident produit complet / +CMEE=2 text ERROR / +CPSI? UE system information / +cgdcont=1,"IP","Free"
  Horloge.begin();  //reglage avec : Horloge.settime(0, 00, 22, 25, 8, 22, 5);  //   sec,  min,  hour, jour, mois , année , joursemaine
}
//*************************************************************
uint16_t tension_read;
uint16_t temp_kelvin;
uint16_t temp_kelvin_tabl[6];
//void process_heure(){  sprintf(szRec,"Heure:%d, sixieme:%d",cptheur,itk);Serial.println(szRec);}
//************************************************************************************************************
#define SIZE_SMS 140
char szSMS[SIZE_SMS];
void ClotureSms() 
{
 int len=strlen(szSMS);
  if (len+4>SIZE_SMS) {len=SIZE_SMS-4; Serial.println("SMS over !!!");}
  szSMS[len]=26;szSMS[len+1]=0;                              //Serial.write("LenSMS");Serial.println(len);
}
//**************************************************************
void SendStat(char * pNum) { // 
    sprintf(szSMS,"%s %dk\r\nMode(%s)\r\nEtat(%s)\r\nSeuil(%d.%d)\r\nT(%d,%d,%d,%d,%d,%d)",Horloge.gettime("Ymd H:i"),temp_kelvin,szModeVanne[mode_vanne], szStatVann[statVanne],kelvin_seuil_purge,kelvin_seuil_alim,temp_kelvin_tabl[0], temp_kelvin_tabl[1], temp_kelvin_tabl[2], temp_kelvin_tabl[3], temp_kelvin_tabl[4], temp_kelvin_tabl[5]);
    ClotureSms();
    if (pNum) SendMessage(pNum);
    else SmsDiffus();
}
//*************************************************************
void loop() {
  cpttime();
  loopSim900Dump();
  loopInput();  
}
//****************************************************
void loopInput() {
 int8_t ok=1;
  cbRec=0;
  while (Serial.available() && ok) 
  {
    while (Serial.available() && ok){ 
    char c=Serial.read();  
      if (c!=10) szRec[cbRec++]=c; 
      else ok=0;
      if (cbRec+2>MAX_REC) ok=0;
    }
    szRec[cbRec]=0;
    delay(100);
  }  

  if (cbRec) 
  {
    szRec[cbRec]=0; 
    Serial.write("User:["); Serial.write(szRec);Serial.write("]"); 
    if ( (szRec[0]==0x41  && szRec[1]==0x54) || (szRec[0]==0x61  && szRec[1]==0x74) )   // commande AT pour module serial GSM
      { Serial.println("SIM command >"); Sim900A.println(szRec); } 
    else { 
        if (strstr(szRec,"#")) { Serial.println("Vanne command >"); CommandeRecept(szRec,"*"); }     // commande # pour automate ALIM/PURGE
        else Serial.println("?commande?"); 
      }
  }
}
//****************************************************
void loopSim900Dump() {
  cbRec=0;
 int8_t ok=1;
  while (Sim900A.available() && ok) 
  {
    while (Sim900A.available() && ok){ 
      szRec[cbRec++]=Sim900A.read();  
      if (cbRec+2>MAX_REC) ok=0;
    }
    delay(100);
  }    
  if (cbRec) {szRec[cbRec]=0; Serial.write("SimIn:["); Serial.write(szRec);Serial.println("]");DetectReceptSms(szRec);}// CommandeRecept(szRec);}
}
//*************************************************************
void kelvin_of_LM335() {
  tension_read = analogRead(1);
  temp_kelvin=(1486 - tension_read)/2;
 }
//*************************************************************
void process_minute() {
  kelvin_of_LM335();
uint8_t itk=Horloge.Hours/4;
  temp_kelvin_tabl[itk]=temp_kelvin;   //sprintf(szRec,"heure:%d, quadr:%d",Horloge.Hours,itk);Serial.println(szRec);
  
 static uint16_t prec_temp=0;
  if (abs(temp_kelvin-prec_temp)>1) {prec_temp=temp_kelvin; Serial.print(Horloge.gettime("Ymd, H:i:s, D"));  Serial.print(" - V :");  Serial.print(tension_read);  Serial.print(" - Temp :");  Serial.print(temp_kelvin);   Serial.println("°k");}

  if (mode_vanne!=MODE_AUTO) return;
 
  if ( temp_kelvin > kelvin_seuil_purge) delai_seuil_purge = 0;
  else  {
    if (statVanne==VANN_ALIM_OK || statVanne==VANN_INIT)
    {      
      if (!delai_seuil_purge) delai_seuil_purge = DELAY_PURGE;
      else                    delai_seuil_purge--;      
      if (!delai_seuil_purge) reqVannePurge();    
    }
  }

  if (temp_kelvin < kelvin_seuil_alim) delai_seuil_alim = 0;
  else  {
    if (statVanne==VANN_PURGE_OK || statVanne==VANN_INIT)
    {
      if (!delai_seuil_alim) delai_seuil_alim = DELAY_ALIM;
      else                   delai_seuil_alim--;
      if (!delai_seuil_alim) reqVanneAlim();      
    }
  }
}
//********************************************************
void reqVannePurge() { 
//char szT[64];
uint32_t time_in=deci_seconde();
  statVanne=VANN_PURGE_REQ;
  digitalWrite(REV_RELAY_OUT, LOW);
 
  while (statVanne==VANN_PURGE_REQ)
  {
    if (deci_seconde()>(time_in+TEMPO_VANNE_DECI_SEC)) statVanne = VANN_PURGE_ERR;
    if (digitalRead(REV_SWITCH_IN) == 1) statVanne = VANN_PURGE_OK; // switch fin de course a bien coupé l'excitation relai purge 
  }
  digitalWrite(REV_RELAY_OUT, HIGH);
  duree_manoeuvre=deci_seconde()-time_in;
  if (statVanne == VANN_PURGE_OK) 
    sprintf(szSMS,"%s:PURGE OK %d",Horloge.gettime("Ymd H:i"),duree_manoeuvre);
  else 
    sprintf(szSMS,"%s:PURGE ERR  %d",Horloge.gettime("Ymd H:i"),duree_manoeuvre); 
  ClotureSms();
  SmsDiffus();
}
//********************************************************
void reqVanneAlim()  {
//char szT[64];
uint32_t time_in=deci_seconde();
  statVanne=VANN_ALIM_REQ;
  digitalWrite(FWD_RELAY_OUT, LOW);

  while (statVanne==VANN_ALIM_REQ)
  {
    if (deci_seconde()>(time_in+TEMPO_VANNE_DECI_SEC)) statVanne = VANN_ALIM_ERR;
    if (digitalRead(FWD_SWITCH_IN) == 1) statVanne = VANN_ALIM_OK; // switch fin de course a bien coupé l'excitation relai purge 
  }
  digitalWrite(FWD_RELAY_OUT, HIGH);
  duree_manoeuvre=deci_seconde()-time_in;
  if (statVanne == VANN_ALIM_OK) 
    sprintf(szSMS,"%s:ALIM OK %d",Horloge.gettime("Ymd H:i"),duree_manoeuvre); 
  else 
    sprintf(szSMS,"%s:ALIM ERR %d",Horloge.gettime("Ymd H:i"),duree_manoeuvre); 
  ClotureSms();
  SmsDiffus();
}
//****************************************************************
void SmsDiffus()//char * pT)                                                  // if (cptjour<2) return; // pour test 
{ 
//SendMessage("+33631881147", pT); //Pierrot
  SendMessage("+33681854641");//, pT); //Ra
  SendMessage("+33631881147");//, pT); //Pierrot
  //SendUdp("Diffus :",pT);
}
//****************************************************************
void DetectReceptSms(char * pT)  
{                             
char szLastNum[16]="";
char * pRecSms=strstr(pT,"+CMT");
  if (pRecSms==NULL) return;
char * pNum=strstr(pT,"\"+33"); 
   if (pNum) 
   {
    pNum++;
   char i=0; while(pNum[i]!=34 && i<14) {szLastNum[i]=pNum[i];i++;} 
    if (i<14) 
    {
      szLastNum[i]=0;   
      Serial.write("Sms from :");Serial.write(szLastNum);Serial.write(" Content:");Serial.println(pNum+i);
      if (*szLastNum) CommandeRecept(pNum+i, szLastNum);
    }
    //SendUdp("SMS In :",pNum);
   }
}
//****************************************************************
void CommandeRecept(char * pT, char * pNum)                                       // commande input ou SMS doit contenir #ALIM, #PURGE ou #AUTO option param .kelvin_seuil_purge.kelvin_seuil_alim : format strict
{    
  if ((strstr(pT,"#STAT") !=0)||(strstr(pT,"#stat") !=0)) {SendStat(pNum);  return; }
  if ((strstr(pT,"#ALIM") !=0)||(strstr(pT,"#alim") !=0)) {mode_vanne=MODE_ALIM;  reqVanneAlim();  return; }
  if ((strstr(pT,"#PURGE")!=0)||(strstr(pT,"#purge")!=0)) {mode_vanne=MODE_PURGE; reqVannePurge(); return; }
  if ((strstr(pT,"#AUTO") !=0)||(strstr(pT,"#auto") !=0))  
  {
   uint16_t vmin=0, vmax=0;
   char * pstr=strstr(pT,"#");
    mode_vanne=MODE_AUTO; 
    if (pstr[5]=='.') 
    {
      vmin=val3(pstr+6);                      
      if (pstr[9]=='.') vmax=val3(pstr+10);                                                               // char szT[32]; sprintf(szT,"vmin:%d vmax:%d",vmin,vmax); Serial.println(szT);
      if (vmin>250  && vmax<300 && (vmin+2)<vmax) {kelvin_seuil_purge=vmin; kelvin_seuil_alim=vmax;}
    }
    SendStat(0);
  }
}
//****************************************************************
uint16_t val3(char * pR)
{
  if (pR[0]<0x30 || pR[0]>0x39 || pR[1]<0x30 || pR[1]>0x39 || pR[2]<0x30 || pR[2]>0x39) return 0;
  return (pR[0]-0x30)*100 + (pR[1]-0x30)*10 + (pR[2]-0x30);  
}
//****************************************************************
//****************************************************
void SendMessage(const char * pNum)//, char * pMess)
{
  Serial.write("SEND[");Serial.write(szSMS);Serial.write("] SMS-> ");  
  if (!pNum || *pNum!='+') {Serial.println("Err Num");return;}
  Serial.println(pNum);

char szT[32];
  sprintf(szT,"AT+CMGS=\"%s\"",pNum); //Sim900A.println(szT); delay(1000); 
 int rep=sendATcommand(szT,">","ERR",6000);             if (rep!=1) {Serial.write("Err > ");    Serial.println(rep); return;}   
     rep=sendATcommand(szSMS,"+CMGS:","ERROR",100000);  if (rep!=1) {Serial.write("Err CMGS "); Serial.println(rep); return;}
}
//**************************************************
int8_t sendATcommand(char* pATcommand, char* pAnswerOk, char* pAnswerErr, unsigned int timeout)
{
uint8_t x=0,  answer=0;
uint64_t previous;
    memset(szRec, '\0', sizeof(szRec));   delay(100);
    while( Sim900A.available() > 0) Serial.write(Sim900A.read());
    Sim900A.println(pATcommand);    
    x = 0;
    previous = millis64();
    Serial.write("{");
    do{        
        if(Sim900A.available() != 0){                
            szRec[x] = Sim900A.read(); Serial.write(szRec[x]);
            x++;  
            if (x+1>=sizeof(szRec)) answer=3;
            if      (strstr(szRec, pAnswerOk) != NULL)      { answer = 1; }  
            else if (strstr(szRec, pAnswerErr) != NULL)     { answer = 2; }  
        }
    }
    while((answer == 0) && ((millis64() - previous) < timeout));    
    Serial.write("}");
    return answer;
}
//**************************************************
/*
char SendUdp_IpInit=0;  
void SendUdp(char * pIntro, char * pMess)// ??? debordement dès setup lorsqu'on declare le contenu sur le uno ????
  //Serial.write(" ?delai? ");  delay(10000);
  if (!SendUdp_IpInit) {
    sendATcommand("AT+CIPMUX=0","OK","ERROR",10000);              //AT+CIPSTATUS -> INITIAL 
    sendATcommand("AT+CSTT=\"Free\"","OK","ERROR",10000);         //AT+CIPSTATUS -> START 
    SendUdp_IpInit=1;
  }

  sendATcommand("AT+CIICR","OK","ERROR",10000);             //AT+CIPSTATUS -> GPRSACT 
  sendATcommand("AT+CIFSR",".","ERROR",10000);             //AT+CIPSTATUS -> IP STATUS 
  sendATcommand("AT+CIPSTART=\"UDP\",\"62.147.214.90\",\"30103\"\r\n","OK","FAIL",4000);

int len=strlen(pIntro)+strlen(pMess)+2;
  sprintf(szT,"AT+CIPSEND=%d",len);
  sendATcommand(szT,">","ERROR",4000);
  Sim900A.write(pIntro); Sim900A.write(pMess);  sendATcommand("\r\n","OK","ERROR",10000);// Sim900A.write("\r\n"); delay(500); 

  sendATcommand("AT+CIPCLOSE","OK","ERROR",10000);   //AT+CIPSTATUS -> UDP CLOSED
  sendATcommand("AT+CIPSHUT","OK","ERROR",10000);    //AT+CIPSTATUS -> IP INITIAL 
}
*/
