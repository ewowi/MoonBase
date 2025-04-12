/**
    @title     MoonBase
    @file      ModuleAnimations.h
    @repo      https://github.com/ewowi/MoonBase, submit changes to this file as PRs
    @Authors   https://github.com/ewowi/MoonBase/commits/main
    @Doc       https://ewowi.github.io/MoonBase/modules/module/animations/
    @Copyright © 2025 Github MoonBase Commit Authors
    @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
    @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
**/

#ifndef ModuleAnimations_h
#define ModuleAnimations_h

#if FT_MOONLIGHT == 1

#include "Module.h"

#undef TAG
#define TAG "💫"

#include "FastLED.h"
#define MAXLEDS 8192

#if FT_LIVESCRIPT
    #include "ESPLiveScript.h" //note: contains declarations AND definitions, therefore can only be included once!
#endif

struct Coord3D {
    int x;
    int y;
    int z;
};

class Node {

};

class EffectNode : public Node {

    public:
    
    Coord3D size = {8,8,1}; //not 0,0,0 to prevent div0 eg in Octopus2D

    void fadeToBlackBy(const uint8_t fadeBy) {}
    void setPixelColor(const Coord3D &pixel, const CRGB& color) {}
};

class ModuleAnimations : public Module
{
public:

    CRGB leds[MAXLEDS];
    uint16_t nrOfLeds = 256;
    #if FT_LIVESCRIPT
        Parser parser = Parser();
    #endif

    ModuleAnimations(PsychicHttpServer *server,
        ESP32SvelteKit *sveltekit,
        FilesService *filesService
    ) : Module("animations", server, sveltekit, filesService) {
        ESP_LOGD(TAG, "constructor");
    }

    void begin() {
        Module::begin();

        ESP_LOGD(TAG, "");

        //create a handler which recompiles the animation when the file of the current animation changes
        _filesService->addUpdateHandler([&](const String &originId)
        { 
            ESP_LOGD(TAG, "FilesService::updateHandler %s", originId.c_str());
            //read the file state
            _filesService->read([&](FilesState &filesState) {
                // loop over all changed files (normally only one)
                for (auto updatedItem : filesState.updatedItems) {
                    //if file is the current animation, recompile it (to do: multiple animations)
                    for (JsonObject node: _state.data["nodes"].as<JsonArray>()) {
                        String animation = node["animation"];

                        if (updatedItem == animation) {
                            ESP_LOGD(TAG, "updateHandler updatedItem %s", updatedItem.c_str());
                            compileAndRun(animation.c_str());
                        }
                    }
                }
            });
        });

        _socket->registerEvent("animationsRO");
    }

    void setupDefinition(JsonArray root) override {
        ESP_LOGD(TAG, "");
        JsonObject property; // state.data has one or more properties
        JsonArray details; // if a property is an array, this is the details of the array
        JsonArray values; // if a property is a select, this is the values of the select

        property = root.add<JsonObject>(); property["name"] = "lightsOn"; property["type"] = "checkbox"; property["default"] = true;
        property = root.add<JsonObject>(); property["name"] = "brightness"; property["type"] = "range"; property["min"] = 0; property["max"] = 255; property["default"] = 10;
        property = root.add<JsonObject>(); property["name"] = "driverOn"; property["type"] = "checkbox"; property["default"] = true;
        property = root.add<JsonObject>(); property["name"] = "pin"; property["type"] = "select"; property["default"] = 2; values = property["values"].to<JsonArray>();
        values.add("2");
        values.add("16");
        property = root.add<JsonObject>(); property["name"] = "millis"; property["type"] = "number";

        property = root.add<JsonObject>(); property["name"] = "nodes"; property["type"] = "array"; details = property["n"].to<JsonArray>();
        {
            property = details.add<JsonObject>(); property["name"] = "animation"; property["type"] = "selectFile"; property["default"] = "Random"; values = property["values"].to<JsonArray>();
            values.add("Random");
            values.add("Sinelon");
            values.add("Rainbow");
            values.add("Sinus");
            values.add("Lissajous");
            //find all the .sc files on FS
            File rootFolder = ESPFS.open("/");
            walkThroughFiles(rootFolder, [&](File folder, File file) {
                if (strstr(file.name(), ".sc")) {
                    // ESP_LOGD(TAG, "found file %s", file.path());
                    values.add((char *)file.path());
                }
            });
            rootFolder.close();
            property = details.add<JsonObject>(); property["name"] = "type"; property["type"] = "select"; property["default"] = "Effect"; values = property["values"].to<JsonArray>();
            values.add("Fixture definition");
            values.add("Fixture mapping");
            values.add("Effect");
            values.add("Modifier");
            values.add("Driver show");
            property = details.add<JsonObject>(); property["name"] = "size"; property["type"] = "number"; property["default"] = 85;
            property = details.add<JsonObject>(); property["name"] = "controls"; property["type"] = "array"; details = property["n"].to<JsonArray>();
            {
                property = details.add<JsonObject>(); property["name"] = "name"; property["type"] = "text"; property["default"] = "speed";
                property = details.add<JsonObject>(); property["name"] = "type"; property["type"] = "select"; property["default"] = "Number"; values = property["values"].to<JsonArray>();
                values.add("Number");
                values.add("Range");
                values.add("Text");
                values.add("Coordinate");
                property = details.add<JsonObject>(); property["name"] = "value"; property["type"] = "text"; property["default"] = "128";
            }
            //, "controls":[{"name":"speed","type":"range", "value":"128"}]
        }

        property = root.add<JsonObject>(); property["name"] = "scripts"; property["type"] = "array"; details = property["n"].to<JsonArray>();
        {
            property = details.add<JsonObject>(); property["name"] = "name"; property["type"] = "text"; property["ro"] = true;
            property = details.add<JsonObject>(); property["name"] = "isRunning"; property["type"] = "checkbox"; property["ro"] = true;
            property = details.add<JsonObject>(); property["name"] = "isHalted"; property["type"] = "checkbox"; property["ro"] = true;
            property = details.add<JsonObject>(); property["name"] = "exeExist"; property["type"] = "checkbox"; property["ro"] = true;
            property = details.add<JsonObject>(); property["name"] = "handle"; property["type"] = "number"; property["ro"] = true;
            property = details.add<JsonObject>(); property["name"] = "binary_size"; property["type"] = "number"; property["ro"] = true;
            property = details.add<JsonObject>(); property["name"] = "data_size"; property["type"] = "number"; property["ro"] = true;
            property = details.add<JsonObject>(); property["name"] = "kill"; property["type"] = "button";
        }
    }

    void onUpdate(UpdatedItem updatedItem) override
    {
        if (equal(updatedItem.name, "pin")) {
            ESP_LOGD(TAG, "handle %s = %s -> %s", updatedItem.name, updatedItem.oldValue.c_str(), updatedItem.value.as<String>().c_str());
            //In constructor so before onUpdate
            switch (updatedItem.value.as<int>()) {
                case 2:
                    FastLED.addLeds<WS2812B, 2, GRB>(leds, 0, nrOfLeds);
                    break;
                case 16:
                    FastLED.addLeds<WS2812B, 16, GRB>(leds, 0, nrOfLeds);
                    break;
                default:
                    ESP_LOGD(TAG, "unknown pin %d", updatedItem.value.as<int>());
            }
            FastLED.setMaxPowerInMilliWatts(10000); // 5v, 2000mA, to protect usb while developing
            FastLED.setBrightness(_state.data["lightsOn"]?_state.data["brightness"]:0);
            ESP_LOGD(TAG, "FastLED.addLeds n:%d", nrOfLeds);
        } else if (equal(updatedItem.name, "lightsOn") || equal(updatedItem.name, "brightness")) {
            ESP_LOGD(TAG, "handle %s = %s -> %s", updatedItem.name, updatedItem.oldValue.c_str(), updatedItem.value.as<String>().c_str());
            FastLED.setBrightness(_state.data["lightsOn"]?_state.data["brightness"]:0);
        } else if (equal(updatedItem.parent[0], "nodes") && equal(updatedItem.name, "animation")) {    
            ESP_LOGD(TAG, "handle %s = %s -> %s", updatedItem.name, updatedItem.oldValue.c_str(), updatedItem.value.as<String>().c_str());
            if (updatedItem.oldValue.length())
                ESP_LOGD(TAG, "delete %s ...", updatedItem.oldValue.c_str());
            if (updatedItem.value.as<String>().length())
                compileAndRun(updatedItem.value.as<String>().c_str());
        } else if (equal(updatedItem.parent[0], "scripts") && equal(updatedItem.name, "kill")) {    
            ESP_LOGD(TAG, "handle %s[%d].%s = %s -> %s", updatedItem.parent[0], updatedItem.index[0], updatedItem.name, updatedItem.oldValue.c_str(), updatedItem.value.as<String>().c_str());
            kill(updatedItem.index[0]);
        } else
            ESP_LOGD(TAG, "no handle for %s = %s -> %s", updatedItem.name, updatedItem.oldValue.c_str(), updatedItem.value.as<String>().c_str());
    }

    //AI generated
    void sinusEffect(CRGB* leds, uint16_t numLeds, uint8_t hueOffset = 0, uint8_t brightness = 255, uint16_t speed = 10) {
        static uint16_t phase = 0; // Tracks the phase of the sine wave
        // ESP_LOGD(TAG, "sinusEffect %d %d %d %d", numLeds, hueOffset, brightness, speed);
    
        for (uint16_t i = 0; i < numLeds; i++) {
            // Calculate the sine wave value for the current LED
            uint8_t wave = sin8((i * 255 / numLeds) + phase);
    
            // Map the sine wave value to a color hue
            uint8_t hue = wave + hueOffset;
    
            // Set the LED color using the calculated hue
            leds[i] = CHSV(hue, 255, brightness);
        }
    
        // Increment the phase to animate the wave
        phase += speed;
    }

    void loop()
    {
        bool showLeds = false;
        
        for (JsonObject node: _state.data["nodes"].as<JsonArray>()) {
            String animation = node["animation"];
            //select the right effect
            if (animation == "Random") {
                fadeToBlackBy(leds, nrOfLeds, 70);
                leds[random16(nrOfLeds)] = CRGB(255, random8(), 0);
                showLeds = true;
            } else if (animation == "Sinelon") {
                fadeToBlackBy(leds, nrOfLeds, 20);
                uint8_t bpm = 60;
                int pos = beatsin16( bpm, 0, 255 );
                leds[pos] += CHSV( millis()/50, 255, 255); //= CRGB(255, random8(), 0);
                showLeds = true;
            } else if (animation == "Rainbow") {
                static uint8_t hue = 0;
                fill_rainbow(leds, nrOfLeds, hue++, 7);
                showLeds = true;
            } else if (animation == "Sinus") {
                fadeToBlackBy(leds, nrOfLeds, 70);
                sinusEffect(leds, nrOfLeds, millis() / 10, 255, 5);
                showLeds = true;
            } else if (animation == "Lissajous") {

                uint8_t xFrequency = 64;// = leds.effectControls.read<uint8_t>();
                uint8_t fadeRate = 128;// = leds.effectControls.read<uint8_t>();
                uint8_t speed = 128;// = leds.effectControls.read<uint8_t>();
                CRGBPalette16 palette = PartyColors_p;

                EffectNode leds;

                leds.fadeToBlackBy(fadeRate);
                uint_fast16_t phase = millis() * speed / 256;  // allow user to control rotation speed, speed between 0 and 255!
                Coord3D locn = {0,0,0};
                for (int i=0; i < 256; i ++) {
                    //WLEDMM: stick to the original calculations of xlocn and ylocn
                    locn.x = sin8(phase/2 + (i*xFrequency)/64);
                    locn.y = cos8(phase/2 + i*2);
                    locn.x = (leds.size.x < 2) ? 1 : (::map(2*locn.x, 0,511, 0,2*(leds.size.x-1)) +1) /2;    // softhack007: "*2 +1" for proper rounding
                    // leds.setPixelColor((uint8_t)xlocn, (uint8_t)ylocn, leds.color_from_palette(sys->now/100+i, false, PALETTE_SOLID_WRAP, 0));
                    // leds[locn] = ColorFromPalette(leds.palette, sys->now/100+i);
                    // leds.setPixelColorPal(locn, millis()/100+i);
                    leds.setPixelColor(locn, ColorFromPalette(palette, millis()/100+i, 255));
                }
                showLeds = true;
            } else {
                //Done by live script (Yves)
            }
        }
        // Serial.printf(" %s", animation.c_str());
        if (showLeds) driverShow();
    }

    void loop1s() {
        JsonDocument newData; //to only send updatedData

        //push read only variables
        //use state.data or newData?

        JsonArray scripts = newData["scripts"].to<JsonArray>(); //to: remove old array

        for (Executable &exec: scriptRuntime._scExecutables) {
            exe_info exeInfo = scriptRuntime.getExecutableInfo(exec.name);
            JsonObject object = scripts.add<JsonObject>();
            object["name"] = exec.name;
            object["isRunning"] = exec.isRunning();
            object["isHalted"] = exec.isHalted;
            object["exeExist"] = exec.exeExist;
            object["handle"] = exec.__run_handle_index;
            object["binary_size"] = exeInfo.binary_size;
            object["data_size"] = exeInfo.data_size;
            object["kill"] = 0;
            // ESP_LOGD(TAG, "scriptRuntime exec %s r:%d h:%d, e:%d h:%d b:%d + d:%d = %d", exec.name.c_str(), exec.isRunning(), exec.isHalted, exec.exeExist, exec.__run_handle_index, exeInfo.binary_size, exeInfo.data_size, exeInfo.total_size);
        }

        //only if changed
        if (_state.data["scripts"] != newData["scripts"]) {
            newData["millis"] = millis()/1000;
            _state.data["scripts"] = newData["scripts"]; //update without compareRecursive -> without handles
            JsonObject newDataObject = newData.as<JsonObject>();
            _socket->emitEvent("animationsRO", newDataObject);
        } else {
            newData["millis"] = millis()/1000;
            newData.remove("scripts");
            JsonObject newDataObject = newData.as<JsonObject>();
            _socket->emitEvent("animationsRO", newDataObject);
        }
            
        // char buffer[256];
        // serializeJson(doc, buffer, sizeof(buffer));
        // ESP_LOGD(TAG, "livescripts %s", buffer);
    }

    void driverShow()
    {
        if (_state.data["driverOn"])
            FastLED.show();
    }

    //ESPLiveScript
    void compileAndRun(const char * animation) {

        #if FT_LIVESCRIPT
        
            ESP_LOGD(TAG, "animation %s", animation);

            if (animation[0] != '/') { //no sc script
                return;
            }

            //if this animation is already running, kill it ...

            // ESP_LOGD(TAG, "killAndFreeRunningProgram %s", animation.c_str());
            // runInLoopTask.push_back([&] { // run in loopTask to avoid stack overflow
            //     scriptRuntime.killAndFreeRunningProgram();
            // });

            //send UI spinner
        
            //run the recompile not in httpd but in main loopTask (otherwise we run out of stack space)
            // runInLoopTask.push_back([&] {
                ESP_LOGD(TAG, "compileAndRun %s", animation);
                File file = ESPFS.open(animation);
                if (file) {
                    std::string scScript = file.readString().c_str();
                    // scScript += "void main(){setup();sync();}";
                    file.close();
        
                    Executable executable = parser.parseScript(&scScript);
                    executable.name = string(animation);
                    ESP_LOGD(TAG, "parsing %s done\n", animation);
                    scriptRuntime.addExe(executable); //if already exists, delete it first
                    ESP_LOGD(TAG, "addExe success %s\n", executable.exeExist?"true":"false");
        
                    if (executable.exeExist)
                        executable.execute("main"); //background task (async - vs sync)

                }
            // });
        #endif
    
        //stop UI spinner
    }

    // ESPLiveScript
    void kill(int index) {
        ESP_LOGD(TAG, "kill %d", index);
        if (index < scriptRuntime._scExecutables.size()) {
            scriptRuntime.kill(scriptRuntime._scExecutables[index].name);
            scriptRuntime.free(scriptRuntime._scExecutables[index].name);
            scriptRuntime.deleteExe(scriptRuntime._scExecutables[index].name);
        }
    }
};

#endif
#endif