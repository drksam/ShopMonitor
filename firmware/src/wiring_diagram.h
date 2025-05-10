#ifndef WIRING_DIAGRAM_H
#define WIRING_DIAGRAM_H

const char* wiring_diagram_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Wiring Diagram</title>
  <style>
    body { 
      font-family: Arial, sans-serif; 
      margin: 0; 
      padding: 20px; 
      background-color: #f8f9fa;
    }
    
    h1 { 
      color: #0066cc; 
      text-align: center;
    }
    
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    }
    
    svg {
      width: 100%;
      height: auto;
      max-width: 800px;
      display: block;
      margin: 0 auto;
    }
    
    .explanation {
      margin-top: 20px;
      padding: 15px;
      border: 1px solid #ddd;
      border-radius: 4px;
      background-color: #f9f9f9;
    }
    
    .pinout {
      margin-top: 20px;
      border-collapse: collapse;
      width: 100%;
    }
    
    .pinout th, .pinout td {
      border: 1px solid #ddd;
      padding: 8px;
      text-align: left;
    }
    
    .pinout tr:nth-child(even) {
      background-color: #f2f2f2;
    }
    
    .pinout th {
      padding-top: 12px;
      padding-bottom: 12px;
      background-color: #0066cc;
      color: white;
    }
    
    .footer {
      margin-top: 30px;
      text-align: center;
      font-size: 0.8em;
      color: #777;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Wiring Diagram</h1>
    
    <svg width="800" height="600" viewBox="0 0 800 600" xmlns="http://www.w3.org/2000/svg">
      <style>
        /* Main components */
        .board { 
          fill: #2c3e50; 
          stroke: #34495e; 
          stroke-width: 2; 
          filter: drop-shadow(3px 3px 5px rgba(0,0,0,0.3));
        }
        .pins { 
          fill: #95a5a6; 
          stroke: #7f8c8d;
          stroke-width: 0.5;
        }
        .pin-hole {
          fill: #444;
          stroke: #333;
          stroke-width: 0.5;
        }
        
        /* Text styles */
        .label { 
          font-family: Arial, sans-serif; 
          font-size: 11px; 
          fill: #000000; 
          font-weight: bold; 
          text-shadow: 0px 0px 3px white, 0px 0px 2px white; 
        }
        .pin-label { 
          font-family: Arial, sans-serif; 
          font-size: 10px; 
          fill: #ffffff; 
          font-weight: bold;
          filter: drop-shadow(0px 0px 1px rgba(0,0,0,0.7));
        }
        .title { 
          font-family: Arial, sans-serif; 
          font-size: 24px; 
          fill: #2c3e50; 
          font-weight: bold;
          filter: drop-shadow(1px 1px 1px rgba(0,0,0,0.2));
        }
        .subtitle { 
          font-family: Arial, sans-serif; 
          font-size: 14px; 
          fill: #7f8c8d; 
        }
        
        /* Color-coded connection wires with realistic appearance */
        .spi-bus { 
          fill: none; 
          stroke: #e74c3c; 
          stroke-width: 2.5; 
          stroke-linecap: round;
          filter: drop-shadow(1px 1px 1px rgba(0,0,0,0.3));
        }
        .rst-line { 
          fill: none; 
          stroke: #8e44ad; 
          stroke-width: 2.5; 
          stroke-linecap: round;
          filter: drop-shadow(1px 1px 1px rgba(0,0,0,0.3));
        }
        .rfid-ss { 
          fill: none; 
          stroke: #f39c12; 
          stroke-width: 2.5; 
          stroke-linecap: round;
          filter: drop-shadow(1px 1px 1px rgba(0,0,0,0.3));
        }
        .relay-wire { 
          fill: none; 
          stroke: #2ecc71; 
          stroke-width: 2.5; 
          stroke-linecap: round;
          filter: drop-shadow(1px 1px 1px rgba(0,0,0,0.3));
        }
        .led-wire { 
          fill: none; 
          stroke: #9b59b6; 
          stroke-width: 2.5; 
          stroke-linecap: round;
          filter: drop-shadow(1px 1px 1px rgba(0,0,0,0.3));
        }
        .activity-wire { 
          fill: none; 
          stroke: #3498db; 
          stroke-width: 2.5; 
          stroke-linecap: round;
          filter: drop-shadow(1px 1px 1px rgba(0,0,0,0.3));
        }
        .button-wire { 
          fill: none; 
          stroke: #1abc9c; 
          stroke-width: 2.5; 
          stroke-linecap: round;
          filter: drop-shadow(1px 1px 1px rgba(0,0,0,0.3));
        }
        
        /* Component styles with more realistic appearance */
        .rfid-module { 
          fill: #e74c3c; 
          stroke: #c0392b; 
          stroke-width: 1.5; 
          filter: drop-shadow(2px 2px 3px rgba(0,0,0,0.3));
        }
        .rfid-label { 
          font-family: Arial, sans-serif; 
          font-size: 12px; 
          fill: #ffffff; 
          font-weight: bold;
          filter: drop-shadow(0px 0px 1px rgba(0,0,0,0.5));
        }
        .relay-module { 
          fill: #2ecc71; 
          stroke: #27ae60; 
          stroke-width: 1.5; 
          filter: drop-shadow(2px 2px 3px rgba(0,0,0,0.3));
        }
        .relay-label { 
          font-family: Arial, sans-serif; 
          font-size: 12px; 
          fill: #ffffff; 
          font-weight: bold;
          filter: drop-shadow(0px 0px 1px rgba(0,0,0,0.5));
        }
        .led-strip { 
          fill: #9b59b6; 
          stroke: #8e44ad; 
          stroke-width: 1.5; 
          filter: drop-shadow(2px 2px 3px rgba(0,0,0,0.3));
        }
        .led-label { 
          font-family: Arial, sans-serif; 
          font-size: 12px; 
          fill: #ffffff; 
          font-weight: bold;
          filter: drop-shadow(0px 0px 1px rgba(0,0,0,0.5));
        }
        .activity-sensor { 
          fill: #3498db; 
          stroke: #2980b9; 
          stroke-width: 1.5; 
          filter: drop-shadow(2px 2px 3px rgba(0,0,0,0.3));
        }
        .activity-label { 
          font-family: Arial, sans-serif; 
          font-size: 12px; 
          fill: #ffffff; 
          font-weight: bold;
          filter: drop-shadow(0px 0px 1px rgba(0,0,0,0.5));
        }
        
        /* Legend styles */
        .legend { 
          font-family: Arial, sans-serif; 
          font-size: 14px; 
          fill: #2c3e50; 
          font-weight: bold; 
        }
        .legend-box { 
          fill: white; 
          stroke: #bdc3c7; 
          stroke-width: 1;
          filter: drop-shadow(2px 2px 3px rgba(0,0,0,0.2));
          rx: 5;
          ry: 5;
        }
        .legend-item { 
          font-family: Arial, sans-serif; 
          font-size: 12px; 
          fill: #2c3e50; 
        }
        .mode-button { 
          fill: #34495e; 
          stroke: #2c3e50; 
          stroke-width: 1.5;
          filter: drop-shadow(1px 1px 2px rgba(0,0,0,0.3));
        }
        .mode-label { 
          font-family: Arial, sans-serif; 
          font-size: 12px; 
          fill: #ffffff; 
          font-weight: bold;
          filter: drop-shadow(0px 0px 1px rgba(0,0,0,0.5));
        }
        .note { 
          font-family: Arial, sans-serif; 
          font-size: 12px; 
          fill: #2c3e50; 
          font-style: italic;
          filter: drop-shadow(0px 0px 1px rgba(255,255,255,0.8));
        }
        
        /* Wire colors in legend */
        .spi-legend { 
          stroke: #e74c3c; 
          stroke-width: 3;
          stroke-linecap: round;
        }
        .rst-legend { 
          stroke: #8e44ad; 
          stroke-width: 3;
          stroke-linecap: round;
        }
        .ss-legend { 
          stroke: #f39c12; 
          stroke-width: 3;
          stroke-linecap: round;
        }
        .relay-legend { 
          stroke: #2ecc71; 
          stroke-width: 3;
          stroke-linecap: round;
        }
        .led-legend { 
          stroke: #9b59b6; 
          stroke-width: 3;
          stroke-linecap: round;
        }
        .activity-legend { 
          stroke: #3498db; 
          stroke-width: 3;
          stroke-linecap: round;
        }
        .pin-bg { 
          fill: #000000; 
          opacity: 0.5; 
          rx: 3; 
          ry: 3;
        }
        
        /* Background */
        .diagram-bg {
          fill: #f9f9f9;
          rx: 10;
          ry: 10;
          stroke: #ddd;
          stroke-width: 1;
        }
      </style>
      
      <!-- Background -->
      <rect x="10" y="10" width="780" height="580" class="diagram-bg" />
      
      <!-- Title -->
      <text x="400" y="30" class="title" text-anchor="middle">ESP32 Multi-Function Node - Wiring Diagram</text>
      <text x="400" y="50" class="subtitle" text-anchor="middle">Machine Monitor | Office RFID Reader | Accessory IO Controller</text>
      
      <!-- ESP32 DevKit Board -->
      <rect x="300" y="150" width="200" height="300" rx="10" class="board" />
      <text x="400" y="170" class="label" text-anchor="middle">ESP32 DEVKIT</text>
      
      <!-- USB Port -->
      <rect x="380" y="150" width="40" height="10" rx="3" fill="#222" />
      
      <!-- ESP32 Module on board -->
      <rect x="350" y="190" width="100" height="80" rx="3" fill="#555" stroke="#333" stroke-width="1" />
      <text x="400" y="230" font-family="Arial" font-size="10" fill="#fff" text-anchor="middle">ESP32</text>
      
      <!-- Left side pins -->
      <g class="pins">
        <rect x="290" y="190" width="20" height="240" />
        <line x1="290" y1="190" x2="290" y2="430" stroke="#34495e" stroke-width="1" />
        <line x1="310" y1="190" x2="310" y2="430" stroke="#34495e" stroke-width="1" />
        
        <!-- Pin holes - left side -->
        <circle cx="300" cy="200" r="3" class="pin-hole" />
        <circle cx="300" cy="220" r="3" class="pin-hole" />
        <circle cx="300" cy="240" r="3" class="pin-hole" />
        <circle cx="300" cy="260" r="3" class="pin-hole" />
        <circle cx="300" cy="280" r="3" class="pin-hole" />
        <circle cx="300" cy="300" r="3" class="pin-hole" />
        <circle cx="300" cy="320" r="3" class="pin-hole" />
        <circle cx="300" cy="340" r="3" class="pin-hole" />
        <circle cx="300" cy="360" r="3" class="pin-hole" />
        <circle cx="300" cy="380" r="3" class="pin-hole" />
        <circle cx="300" cy="400" r="3" class="pin-hole" />
        <circle cx="300" cy="420" r="3" class="pin-hole" />
        
        <!-- Pin labels - left side with more space -->
        <text x="270" y="200" class="label" text-anchor="end">3.3V</text>
        <text x="270" y="220" class="label" text-anchor="end">GND</text>
        <text x="270" y="240" class="label" text-anchor="end">GPIO15 (Buzzer)</text>
        <text x="270" y="260" class="label" text-anchor="end">GPIO2</text>
        <text x="270" y="280" class="label" text-anchor="end">GPIO4 (RFID4)</text>
        <text x="270" y="300" class="label" text-anchor="end">GPIO16 (RFID3)</text>
        <text x="270" y="320" class="label" text-anchor="end">GPIO17 (RFID2)</text>
        <text x="270" y="340" class="label" text-anchor="end">GPIO5 (LED)</text>
        <text x="270" y="360" class="label" text-anchor="end">GPIO18 (SCK)</text>
        <text x="270" y="380" class="label" text-anchor="end">GPIO19 (MISO)</text>
        <text x="270" y="400" class="label" text-anchor="end">GPIO21 (RFID1)</text>
        <text x="270" y="420" class="label" text-anchor="end">GPIO22 (RST)</text>
      </g>
      
      <!-- Right side pins -->
      <g class="pins">
        <rect x="490" y="190" width="20" height="240" />
        <line x1="490" y1="190" x2="490" y2="430" stroke="#34495e" stroke-width="1" />
        <line x1="510" y1="190" x2="510" y2="430" stroke="#34495e" stroke-width="1" />
        
        <!-- Pin holes - right side -->
        <circle cx="500" cy="200" r="3" class="pin-hole" />
        <circle cx="500" cy="220" r="3" class="pin-hole" />
        <circle cx="500" cy="240" r="3" class="pin-hole" />
        <circle cx="500" cy="260" r="3" class="pin-hole" />
        <circle cx="500" cy="280" r="3" class="pin-hole" />
        <circle cx="500" cy="300" r="3" class="pin-hole" />
        <circle cx="500" cy="320" r="3" class="pin-hole" />
        <circle cx="500" cy="340" r="3" class="pin-hole" />
        <circle cx="500" cy="360" r="3" class="pin-hole" />
        <circle cx="500" cy="380" r="3" class="pin-hole" />
        <circle cx="500" cy="400" r="3" class="pin-hole" />
        <circle cx="500" cy="420" r="3" class="pin-hole" />
        
        <!-- Pin labels - right side with more space -->
        <text x="530" y="200" class="label" text-anchor="start">5V</text>
        <text x="530" y="220" class="label" text-anchor="start">GND</text>
        <text x="530" y="240" class="label" text-anchor="start">GPIO23 (MOSI)</text>
        <text x="530" y="260" class="label" text-anchor="start">GPIO13 (Relay1)</text>
        <text x="530" y="280" class="label" text-anchor="start">GPIO12 (Relay2)</text>
        <text x="530" y="300" class="label" text-anchor="start">GPIO14 (Relay3)</text>
        <text x="530" y="320" class="label" text-anchor="start">GPIO27 (Relay4)</text>
        <text x="530" y="340" class="label" text-anchor="start">GPIO33 (Button1)</text>
        <text x="530" y="360" class="label" text-anchor="start">GPIO32 (Button2)</text>
        <text x="530" y="380" class="label" text-anchor="start">GPIO36 (Activity1)</text>
        <text x="530" y="400" class="label" text-anchor="start">GPIO39 (Activity2)</text>
        <text x="530" y="420" class="label" text-anchor="start">GPIO34/35 (Activity3/4)</text>
      </g>
      
      <!-- RFID Reader Modules with more realistic details -->
      <!-- RFID Reader 1 -->
      <g>
        <rect x="80" y="120" width="120" height="80" rx="5" class="rfid-module" />
        <text x="140" y="140" class="rfid-label" text-anchor="middle">RFID Reader 1</text>
        <text x="140" y="160" class="rfid-label" text-anchor="middle">(Zone 1)</text>
        
        <!-- RFID chip -->
        <rect x="105" y="170" width="70" height="20" rx="2" fill="#333" stroke="#222" stroke-width="1" />
        
        <!-- RFID antenna coil visual -->
        <rect x="115" y="140" width="50" height="50" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        <rect x="120" y="145" width="40" height="40" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        <rect x="125" y="150" width="30" height="30" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        
        <!-- Connector pins -->
        <rect x="170" y="135" width="20" height="50" rx="1" fill="#555" stroke="#333" stroke-width="1" />
        <line x1="180" y1="140" x2="180" y2="180" stroke="#222" stroke-width="1" />
        <circle cx="180" cy="140" r="2" fill="#999" />
        <circle cx="180" cy="150" r="2" fill="#999" />
        <circle cx="180" cy="160" r="2" fill="#999" />
        <circle cx="180" cy="170" r="2" fill="#999" />
        <circle cx="180" cy="180" r="2" fill="#999" />
      </g>
      
      <!-- RFID Reader 2 -->
      <g>
        <rect x="80" y="220" width="120" height="80" rx="5" class="rfid-module" />
        <text x="140" y="240" class="rfid-label" text-anchor="middle">RFID Reader 2</text>
        <text x="140" y="260" class="rfid-label" text-anchor="middle">(Zone 2)</text>
        
        <!-- RFID chip -->
        <rect x="105" y="270" width="70" height="20" rx="2" fill="#333" stroke="#222" stroke-width="1" />
        
        <!-- RFID antenna coil visual -->
        <rect x="115" y="240" width="50" height="50" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        <rect x="120" y="245" width="40" height="40" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        <rect x="125" y="250" width="30" height="30" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        
        <!-- Connector pins -->
        <rect x="170" y="235" width="20" height="50" rx="1" fill="#555" stroke="#333" stroke-width="1" />
        <line x1="180" y1="240" x2="180" y2="280" stroke="#222" stroke-width="1" />
        <circle cx="180" cy="240" r="2" fill="#999" />
        <circle cx="180" cy="250" r="2" fill="#999" />
        <circle cx="180" cy="260" r="2" fill="#999" />
        <circle cx="180" cy="270" r="2" fill="#999" />
        <circle cx="180" cy="280" r="2" fill="#999" />
      </g>
      
      <!-- RFID Reader 3 -->
      <g>
        <rect x="80" y="320" width="120" height="80" rx="5" class="rfid-module" />
        <text x="140" y="340" class="rfid-label" text-anchor="middle">RFID Reader 3</text>
        <text x="140" y="360" class="rfid-label" text-anchor="middle">(Zone 3)</text>
        
        <!-- RFID chip -->
        <rect x="105" y="370" width="70" height="20" rx="2" fill="#333" stroke="#222" stroke-width="1" />
        
        <!-- RFID antenna coil visual -->
        <rect x="115" y="340" width="50" height="50" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        <rect x="120" y="345" width="40" height="40" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        <rect x="125" y="350" width="30" height="30" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        
        <!-- Connector pins -->
        <rect x="170" y="335" width="20" height="50" rx="1" fill="#555" stroke="#333" stroke-width="1" />
        <line x1="180" y1="340" x2="180" y2="380" stroke="#222" stroke-width="1" />
        <circle cx="180" cy="340" r="2" fill="#999" />
        <circle cx="180" cy="350" r="2" fill="#999" />
        <circle cx="180" cy="360" r="2" fill="#999" />
        <circle cx="180" cy="370" r="2" fill="#999" />
        <circle cx="180" cy="380" r="2" fill="#999" />
      </g>
      
      <!-- RFID Reader 4 -->
      <g>
        <rect x="80" y="420" width="120" height="80" rx="5" class="rfid-module" />
        <text x="140" y="440" class="rfid-label" text-anchor="middle">RFID Reader 4</text>
        <text x="140" y="460" class="rfid-label" text-anchor="middle">(Zone 4)</text>
        
        <!-- RFID chip -->
        <rect x="105" y="470" width="70" height="20" rx="2" fill="#333" stroke="#222" stroke-width="1" />
        
        <!-- RFID antenna coil visual -->
        <rect x="115" y="440" width="50" height="50" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        <rect x="120" y="445" width="40" height="40" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        <rect x="125" y="450" width="30" height="30" rx="2" fill="none" stroke="#fff" stroke-width="1" opacity="0.5" />
        
        <!-- Connector pins -->
        <rect x="170" y="435" width="20" height="50" rx="1" fill="#555" stroke="#333" stroke-width="1" />
        <line x1="180" y1="440" x2="180" y2="480" stroke="#222" stroke-width="1" />
        <circle cx="180" cy="440" r="2" fill="#999" />
        <circle cx="180" cy="450" r="2" fill="#999" />
        <circle cx="180" cy="460" r="2" fill="#999" />
        <circle cx="180" cy="470" r="2" fill="#999" />
        <circle cx="180" cy="480" r="2" fill="#999" />
      </g>
      
      <!-- Connection lines for RFID Readers (Slave Select) -->
      <path d="M200 140 C240 140, 270 400, 290 400" class="rfid-ss" />
      <path d="M200 240 C240 240, 265 320, 290 320" class="rfid-ss" />
      <path d="M200 340 C240 340, 265 300, 290 300" class="rfid-ss" />
      <path d="M200 440 C240 440, 265 280, 290 280" class="rfid-ss" />
      
      <!-- Shared RST line -->
      <path d="M195 160 C240 160, 265 420, 290 420" class="rst-line" />
      <path d="M195 260 L195 160" class="rst-line" />
      <path d="M195 360 L195 260" class="rst-line" />
      <path d="M195 460 L195 360" class="rst-line" />
      
      <!-- SPI Bus lines - MISO -->
      <path d="M210 350 C255 350, 265 380, 290 380" class="spi-bus" />
      <path d="M210 350 L210 450" class="spi-bus" />
      <path d="M210 250 L210 350" class="spi-bus" />
      <path d="M210 150 L210 250" class="spi-bus" />
      
      <!-- SPI Bus lines - MOSI -->
      <path d="M220 240 C270 240, 480 240, 490 240" class="spi-bus" />
      <path d="M220 240 L220 450" class="spi-bus" />
      <path d="M220 350 L220 450" class="spi-bus" />
      <path d="M220 250 L220 350" class="spi-bus" />
      <path d="M220 150 L220 250" class="spi-bus" />
      
      <!-- SPI Bus lines - SCK -->
      <path d="M230 355 C240 355, 265 360, 290 360" class="spi-bus" />
      <path d="M230 355 L230 450" class="spi-bus" />
      <path d="M230 255 L230 355" class="spi-bus" />
      <path d="M230 155 L230 255" class="spi-bus" />
      
      <!-- Relay Modules with more realistic details -->
      <!-- Relay 1 -->
      <g>
        <rect x="600" y="120" width="120" height="80" rx="5" class="relay-module" />
        <text x="660" y="140" class="relay-label" text-anchor="middle">Relay 1</text>
        <text x="660" y="160" class="relay-label" text-anchor="middle">(Zone 1)</text>
        
        <!-- Relay components -->
        <rect x="620" y="170" width="80" height="20" rx="2" fill="#1a7a45" stroke="#166437" stroke-width="1" />
        
        <!-- Relay contacts -->
        <circle cx="630" cy="150" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        <circle cx="650" cy="150" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        <circle cx="670" cy="150" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        
        <!-- Status LED -->
        <circle cx="690" cy="150" r="3" fill="#e74c3c" />
        
        <!-- Terminal blocks -->
        <rect x="610" y="170" width="10" height="20" fill="#333" />
        <rect x="630" y="170" width="10" height="20" fill="#333" />
        <rect x="650" y="170" width="10" height="20" fill="#333" />
        <rect x="670" y="170" width="10" height="20" fill="#333" />
        <rect x="690" y="170" width="10" height="20" fill="#333" />
      </g>
      
      <!-- Relay 2 -->
      <g>
        <rect x="600" y="220" width="120" height="80" rx="5" class="relay-module" />
        <text x="660" y="240" class="relay-label" text-anchor="middle">Relay 2</text>
        <text x="660" y="260" class="relay-label" text-anchor="middle">(Zone 2)</text>
        
        <!-- Relay components -->
        <rect x="620" y="270" width="80" height="20" rx="2" fill="#1a7a45" stroke="#166437" stroke-width="1" />
        
        <!-- Relay contacts -->
        <circle cx="630" cy="250" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        <circle cx="650" cy="250" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        <circle cx="670" cy="250" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        
        <!-- Status LED -->
        <circle cx="690" cy="250" r="3" fill="#e74c3c" />
        
        <!-- Terminal blocks -->
        <rect x="610" y="270" width="10" height="20" fill="#333" />
        <rect x="630" y="270" width="10" height="20" fill="#333" />
        <rect x="650" y="270" width="10" height="20" fill="#333" />
        <rect x="670" y="270" width="10" height="20" fill="#333" />
        <rect x="690" y="270" width="10" height="20" fill="#333" />
      </g>
      
      <!-- Relay 3 -->
      <g>
        <rect x="600" y="320" width="120" height="80" rx="5" class="relay-module" />
        <text x="660" y="340" class="relay-label" text-anchor="middle">Relay 3</text>
        <text x="660" y="360" class="relay-label" text-anchor="middle">(Zone 3)</text>
        
        <!-- Relay components -->
        <rect x="620" y="370" width="80" height="20" rx="2" fill="#1a7a45" stroke="#166437" stroke-width="1" />
        
        <!-- Relay contacts -->
        <circle cx="630" cy="350" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        <circle cx="650" cy="350" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        <circle cx="670" cy="350" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        
        <!-- Status LED -->
        <circle cx="690" cy="350" r="3" fill="#e74c3c" />
        
        <!-- Terminal blocks -->
        <rect x="610" y="370" width="10" height="20" fill="#333" />
        <rect x="630" y="370" width="10" height="20" fill="#333" />
        <rect x="650" y="370" width="10" height="20" fill="#333" />
        <rect x="670" y="370" width="10" height="20" fill="#333" />
        <rect x="690" y="370" width="10" height="20" fill="#333" />
      </g>
      
      <!-- Relay 4 -->
      <g>
        <rect x="600" y="420" width="120" height="80" rx="5" class="relay-module" />
        <text x="660" y="440" class="relay-label" text-anchor="middle">Relay 4</text>
        <text x="660" y="460" class="relay-label" text-anchor="middle">(Zone 4)</text>
        
        <!-- Relay components -->
        <rect x="620" y="470" width="80" height="20" rx="2" fill="#1a7a45" stroke="#166437" stroke-width="1" />
        
        <!-- Relay contacts -->
        <circle cx="630" cy="450" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        <circle cx="650" cy="450" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        <circle cx="670" cy="450" r="5" fill="#eee" stroke="#ccc" stroke-width="1" />
        
        <!-- Status LED -->
        <circle cx="690" cy="450" r="3" fill="#e74c3c" />
        
        <!-- Terminal blocks -->
        <rect x="610" y="470" width="10" height="20" fill="#333" />
        <rect x="630" y="470" width="10" height="20" fill="#333" />
        <rect x="650" y="470" width="10" height="20" fill="#333" />
        <rect x="670" y="470" width="10" height="20" fill="#333" />
        <rect x="690" y="470" width="10" height="20" fill="#333" />
      </g>
      
      <!-- Connection lines for Relay Modules -->
      <path d="M510 260 C530 260, 570 140, 600 140" class="relay-wire" />
      <path d="M510 280 C530 280, 570 240, 600 240" class="relay-wire" />
      <path d="M510 300 C530 300, 570 340, 600 340" class="relay-wire" />
      <path d="M510 320 C530 320, 570 440, 600 440" class="relay-wire" />
      
      <!-- LED Strip with more realistic details -->
      <g>
        <rect x="340" y="500" width="120" height="40" rx="5" class="led-strip" />
        <text x="400" y="518" class="led-label" text-anchor="middle">WS2812B LED Strip</text>
        
        <!-- Individual LEDs on strip -->
        <rect x="350" y="525" width="15" height="10" rx="2" fill="#222" stroke="#111" stroke-width="0.5" />
        <rect x="375" y="525" width="15" height="10" rx="2" fill="#222" stroke="#111" stroke-width="0.5" />
        <rect x="400" y="525" width="15" height="10" rx="2" fill="#222" stroke="#111" stroke-width="0.5" />
        <rect x="425" y="525" width="15" height="10" rx="2" fill="#222" stroke="#111" stroke-width="0.5" />
        <rect x="450" y="525" width="15" height="10" rx="2" fill="#222" stroke="#111" stroke-width="0.5" />
        
        <!-- LED dots (lit) -->
        <circle cx="357" cy="530" r="3" fill="#f1c40f" />
        <circle cx="382" cy="530" r="3" fill="#3498db" />
        <circle cx="407" cy="530" r="3" fill="#e74c3c" />
        <circle cx="432" cy="530" r="3" fill="#2ecc71" />
        <circle cx="457" cy="530" r="3" fill="#9b59b6" />
        
        <!-- Connection points -->
        <circle cx="340" cy="520" r="2" fill="#fff" stroke="#777" stroke-width="0.5" />
        <text x="335" y="523" class="pin-label" font-size="6" text-anchor="end">DIN</text>
        
        <circle cx="460" cy="520" r="2" fill="#fff" stroke="#777" stroke-width="0.5" />
        <text x="465" y="523" class="pin-label" font-size="6" text-anchor="start">DOUT</text>
      </g>
      
      <!-- LED connection line -->
      <path d="M400 500 L400 480 C400 460, 330 340, 290 340" class="led-wire" />
      
      <!-- Activity Sensors with more realistic details -->
      <!-- Activity Sensor 1 -->
      <g class="activity-sensor">
        <circle cx="560" cy="380" r="15" />
        <text x="560" y="384" class="activity-label" text-anchor="middle">A1</text>
        
        <!-- Sensor details -->
        <circle cx="560" cy="380" r="8" fill="#2980b9" stroke="#fff" stroke-width="0.5" opacity="0.7" />
        <circle cx="560" cy="380" r="4" fill="#fff" opacity="0.5" />
        
        <!-- Mounting holes -->
        <circle cx="550" cy="370" r="2" fill="#222" />
        <circle cx="570" cy="370" r="2" fill="#222" />
        <circle cx="550" cy="390" r="2" fill="#222" />
        <circle cx="570" cy="390" r="2" fill="#222" />
      </g>
      
      <!-- Activity Sensor 2 -->
      <g class="activity-sensor">
        <circle cx="590" cy="380" r="15" />
        <text x="590" y="384" class="activity-label" text-anchor="middle">A2</text>
        
        <!-- Sensor details -->
        <circle cx="590" cy="380" r="8" fill="#2980b9" stroke="#fff" stroke-width="0.5" opacity="0.7" />
        <circle cx="590" cy="380" r="4" fill="#fff" opacity="0.5" />
        
        <!-- Mounting holes -->
        <circle cx="580" cy="370" r="2" fill="#222" />
        <circle cx="600" cy="370" r="2" fill="#222" />
        <circle cx="580" cy="390" r="2" fill="#222" />
        <circle cx="600" cy="390" r="2" fill="#222" />
      </g>
      
      <!-- Activity Sensor 3 -->
      <g class="activity-sensor">
        <circle cx="620" cy="380" r="15" />
        <text x="620" y="384" class="activity-label" text-anchor="middle">A3</text>
        
        <!-- Sensor details -->
        <circle cx="620" cy="380" r="8" fill="#2980b9" stroke="#fff" stroke-width="0.5" opacity="0.7" />
        <circle cx="620" cy="380" r="4" fill="#fff" opacity="0.5" />
        
        <!-- Mounting holes -->
        <circle cx="610" cy="370" r="2" fill="#222" />
        <circle cx="630" cy="370" r="2" fill="#222" />
        <circle cx="610" cy="390" r="2" fill="#222" />
        <circle cx="630" cy="390" r="2" fill="#222" />
      </g>
      
      <!-- Activity Sensor 4 -->
      <g class="activity-sensor">
        <circle cx="650" cy="380" r="15" />
        <text x="650" y="384" class="activity-label" text-anchor="middle">A4</text>
        
        <!-- Sensor details -->
        <circle cx="650" cy="380" r="8" fill="#2980b9" stroke="#fff" stroke-width="0.5" opacity="0.7" />
        <circle cx="650" cy="380" r="4" fill="#fff" opacity="0.5" />
        
        <!-- Mounting holes -->
        <circle cx="640" cy="370" r="2" fill="#222" />
        <circle cx="660" cy="370" r="2" fill="#222" />
        <circle cx="640" cy="390" r="2" fill="#222" />
        <circle cx="660" cy="390" r="2" fill="#222" />
      </g>
      
      <!-- Activity Sensor connection lines -->
      <path d="M560 395 L560 420 C560 430, 520 380, 510 380" class="activity-wire" />
      <path d="M590 395 L590 420 C590 430, 530 400, 510 400" class="activity-wire" />
      <path d="M620 395 L620 420 C620 430, 530 420, 510 420" class="activity-wire" />
      <path d="M650 395 L650 420 C650 430, 530 420, 510 420" class="activity-wire" />
      
      <!-- Mode Selection Buttons -->
      <rect x="400" y="70" width="40" height="40" rx="20" class="mode-button" />
      <text x="420" y="95" class="mode-label" text-anchor="middle">B1</text>
      
      <rect x="450" y="70" width="40" height="40" rx="20" class="mode-button" />
      <text x="470" y="95" class="mode-label" text-anchor="middle">B2</text>
      
      <!-- Button connection lines -->
      <path d="M420 110 L420 130 C420 140, 490 340, 510 340" class="button-wire" />
      <path d="M470 110 L470 130 C470 140, 490 360, 510 360" class="button-wire" />
      
      <!-- Component Legend -->
      <rect x="30" y="30" width="200" height="110" rx="5" class="legend-box" />
      <text x="40" y="50" class="legend" font-weight="bold">Component Legend</text>
      
      <rect x="40" y="60" width="15" height="15" class="rfid-module" />
      <text x="60" y="72" class="legend-item">RFID Reader</text>
      
      <rect x="40" y="80" width="15" height="15" class="relay-module" />
      <text x="60" y="92" class="legend-item">Relay Module</text>
      
      <rect x="40" y="100" width="15" height="15" class="led-strip" />
      <text x="60" y="112" class="legend-item">LED Indicator</text>
      
      <circle cx="47" cy="127" r="7" class="activity-sensor" />
      <text x="60" y="132" class="legend-item">Activity Sensor</text>
      
      <!-- Wire Legend -->
      <rect x="570" y="30" width="200" height="110" rx="5" class="legend-box" />
      <text x="580" y="50" class="legend" font-weight="bold">Wire Legend</text>
      
      <line x1="580" y1="65" x2="610" y2="65" class="spi-bus" />
      <text x="620" y="70" class="legend-item">SPI Bus (MOSI/MISO/SCK)</text>
      
      <line x1="580" y1="85" x2="610" y2="85" class="rfid-ss" />
      <text x="620" y="90" class="legend-item">RFID SS (Chip Select)</text>
      
      <line x1="580" y1="105" x2="610" y2="105" class="rst-line" />
      <text x="620" y="110" class="legend-item">RFID RST (Reset)</text>
      
      <line x1="580" y1="125" x2="610" y2="125" class="relay-wire" />
      <text x="620" y="130" class="legend-item">Relay Control</text>
      
      <!-- Additional Wire Legend -->
      <rect x="570" y="150" width="200" height="70" rx="5" class="legend-box" />
      
      <line x1="580" y1="170" x2="610" y2="170" class="activity-wire" />
      <text x="620" y="175" class="legend-item">Activity Input</text>
      
      <line x1="580" y1="190" x2="610" y2="190" class="led-wire" />
      <text x="620" y="195" class="legend-item">LED Data</text>
      
      <line x1="580" y1="210" x2="610" y2="210" class="button-wire" />
      <text x="620" y="215" class="legend-item">Mode Selection Buttons</text>
      
      <text x="400" y="560" class="note" text-anchor="middle">Note: This diagram shows all components for a full Machine Monitor node.</text>
      <text x="400" y="580" class="note" text-anchor="middle">Office Reader requires only one RFID reader. Accessory IO uses relay outputs and activity inputs.</text>
    </svg>
    
    <div class="explanation">
      <h2>Pin Connections</h2>
      <p>This diagram shows the wiring connections for the ESP32 Multi-Function Node. The ESP32 DevKit V1 board is connected to multiple peripherals:</p>
      
      <table class="pinout">
        <tr>
          <th>ESP32 Pin</th>
          <th>Connected To</th>
          <th>Function</th>
        </tr>
        <tr>
          <td>GPIO21</td>
          <td>RFID Reader 1 SS</td>
          <td>RFID Reader 1 Chip Select</td>
        </tr>
        <tr>
          <td>GPIO17</td>
          <td>RFID Reader 2 SS</td>
          <td>RFID Reader 2 Chip Select</td>
        </tr>
        <tr>
          <td>GPIO16</td>
          <td>RFID Reader 3 SS</td>
          <td>RFID Reader 3 Chip Select</td>
        </tr>
        <tr>
          <td>GPIO4</td>
          <td>RFID Reader 4 SS</td>
          <td>RFID Reader 4 Chip Select</td>
        </tr>
        <tr>
          <td>GPIO22</td>
          <td>All RFID Readers RST</td>
          <td>RFID Reader Reset (shared)</td>
        </tr>
        <tr>
          <td>GPIO13</td>
          <td>Relay 1</td>
          <td>Zone 1 Machine Control</td>
        </tr>
        <tr>
          <td>GPIO12</td>
          <td>Relay 2</td>
          <td>Zone 2 Machine Control</td>
        </tr>
        <tr>
          <td>GPIO14</td>
          <td>Relay 3</td>
          <td>Zone 3 Machine Control</td>
        </tr>
        <tr>
          <td>GPIO27</td>
          <td>Relay 4</td>
          <td>Zone 4 Machine Control</td>
        </tr>
        <tr>
          <td>GPIO5</td>
          <td>WS2812B LED Strip</td>
          <td>Status Indicator</td>
        </tr>
        <tr>
          <td>GPIO36</td>
          <td>Activity Sensor 1</td>
          <td>Machine 1 Activity Counter</td>
        </tr>
        <tr>
          <td>GPIO39</td>
          <td>Activity Sensor 2</td>
          <td>Machine 2 Activity Counter</td>
        </tr>
        <tr>
          <td>GPIO34</td>
          <td>Activity Sensor 3</td>
          <td>Machine 3 Activity Counter</td>
        </tr>
        <tr>
          <td>GPIO35</td>
          <td>Activity Sensor 4</td>
          <td>Machine 4 Activity Counter</td>
        </tr>
        <tr>
          <td>GPIO33</td>
          <td>Button 1</td>
          <td>Mode Selection</td>
        </tr>
        <tr>
          <td>GPIO32</td>
          <td>Button 2</td>
          <td>Mode Selection</td>
        </tr>
        <tr>
          <td>GPIO15</td>
          <td>Buzzer</td>
          <td>Audio Feedback</td>
        </tr>
      </table>
    </div>
    
    <div class="footer">
      <p>Shop ESP32 Multi-Function Node v1.0.0 | &copy; 2025</p>
    </div>
  </div>
</body>
</html>
)rawliteral";

#endif // WIRING_DIAGRAM_H