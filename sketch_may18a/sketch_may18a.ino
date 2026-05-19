const int analogPin = A0;

void setup() {
  Serial.begin(9600);
  
  // Optional: Label the graph variable. 
  // This will show up in the Serial Plotter legend.
  Serial.println("Voltage(V)"); 
}

void loop() {
  // Read the raw 10-bit ADC value (0 to 1023)
  int rawADC = analogRead(analogPin);
  
  // Convert to actual voltage (0V to 5V)
  float voltage = (rawADC * 5.0) / 1023.0;
  
  // Print ONLY the number so the plotter can graph it
  Serial.println(voltage);
  
  // A small 50ms delay keeps the graph smooth and scrolling nicely
  delay(50);
}