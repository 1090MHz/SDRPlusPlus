#include <imgui.h>
#include <watcher.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <radio_demod.h>
#include <new_module.h>
#include <wfm_demod.h>
#include <fm_demod.h>
#include <am_demod.h>
#include <usb_demod.h>
#include <lsb_demod.h>
#include <dsb_demod.h>
#include <raw_demod.h>
#include <cw_demod.h>

#define CONCAT(a, b)    ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "radio",
    /* Description:     */ "Radio module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 3, 0,
    /* Max instances    */ -1
};

static ConfigManager config;

class RadioModule : public ModuleManager::Instance {
public:
    RadioModule(std::string name) {
        this->name = name;

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 200000, 200000, 1);

        ns.init(vfo->output);

        config.aquire();
        if (!config.conf.contains(name)) {
            config.conf[name]["selectedDemodId"] = 1;
        }
        demodId = config.conf[name]["selectedDemodId"];
        config.release(true);

        wfmDemod.init(name, vfo, audioSampRate, 200000, &config);
        fmDemod.init(name, vfo, audioSampRate, 12500, &config);
        amDemod.init(name, vfo, audioSampRate, 12500, &config);
        usbDemod.init(name, vfo, audioSampRate, 3000, &config);
        lsbDemod.init(name, vfo, audioSampRate, 3000, &config);
        dsbDemod.init(name, vfo, audioSampRate, 6000, &config);
        rawDemod.init(name, vfo, audioSampRate, audioSampRate, &config);
        cwDemod.init(name, vfo, audioSampRate, 200, &config);
        
        srChangeHandler.ctx = this;
        srChangeHandler.handler = sampleRateChangeHandler;
        stream.init(wfmDemod.getOutput(), srChangeHandler, audioSampRate);
        sigpath::sinkManager.registerStream(name, &stream);

        // TODO: Replace with config load
        demodId = 1;
        selectDemod(&wfmDemod);

        // TODO: Remove the two above lines when implemented
        // selectDemodulatorByID(demodId);

        stream.start();

        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~RadioModule() {
        
    }

    void enable() {
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 200000, 200000, 1);
        //ns.stop();
        currentDemod->setVFO(vfo);
        currentDemod->select();
        currentDemod->start();
        enabled = true;
    }

    void disable() {
        currentDemod->stop();
        sigpath::vfoManager.deleteVFO(vfo);
        //ns.setInput(vfo->output);
        //ns.start();
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuHandler(void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;

        if (!_this->enabled) { style::beginDisabled(); }

        float menuWidth = ImGui::GetContentRegionAvailWidth();
        ImGui::BeginGroup();

        // TODO: Change VFO ref in signal path

        ImGui::Columns(4, CONCAT("RadioModeColumns##_", _this->name), false);
        if (ImGui::RadioButton(CONCAT("NFM##_", _this->name), _this->demodId == 0) && _this->demodId != 0) { 
            _this->demodId = 0;
            _this->selectDemod(&_this->fmDemod);
        }
        if (ImGui::RadioButton(CONCAT("WFM##_", _this->name), _this->demodId == 1) && _this->demodId != 1) {
            _this->demodId = 1;
            _this->selectDemod(&_this->wfmDemod);
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("AM##_", _this->name), _this->demodId == 2) && _this->demodId != 2) {
            _this->demodId = 2;
            _this->selectDemod(&_this->amDemod);
        }
        if (ImGui::RadioButton(CONCAT("DSB##_", _this->name), _this->demodId == 3) && _this->demodId != 3)  {
            _this->demodId = 3;
            _this->selectDemod(&_this->dsbDemod);
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("USB##_", _this->name), _this->demodId == 4) && _this->demodId != 4) {
            _this->demodId = 4;
            _this->selectDemod(&_this->usbDemod);
        }
        if (ImGui::RadioButton(CONCAT("CW##_", _this->name), _this->demodId == 5) && _this->demodId != 5) {
            _this->demodId = 5;
            _this->selectDemod(&_this->cwDemod);
        };
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("LSB##_", _this->name), _this->demodId == 6) && _this->demodId != 6) {
            _this->demodId = 6;
            _this->selectDemod(&_this->lsbDemod);
        }
        if (ImGui::RadioButton(CONCAT("RAW##_", _this->name), _this->demodId == 7) && _this->demodId != 7) {
            _this->demodId = 7;
            _this->selectDemod(&_this->rawDemod);
        };
        ImGui::Columns(1, CONCAT("EndRadioModeColumns##_", _this->name), false);

        ImGui::EndGroup();

        _this->currentDemod->showMenu();

        if (!_this->enabled) { style::endDisabled(); }
    }

    static void sampleRateChangeHandler(float sampleRate, void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;
        // TODO: If too slow, change all demods here and not when setting
        _this->audioSampRate = sampleRate;
        if (_this->currentDemod != NULL) {
            _this->currentDemod->setAudioSampleRate(_this->audioSampRate);
        }
    }

    void selectDemod(Demodulator* demod) {
        if (currentDemod != NULL) { currentDemod->stop(); }
        currentDemod = demod;
        currentDemod->setAudioSampleRate(audioSampRate);
        stream.setInput(currentDemod->getOutput());
        currentDemod->select();
        vfo->output->flush();
        currentDemod->start();
    }

    void selectDemodByID(int id) {
        // TODO: Implement
    }

    std::string name;
    bool enabled = true;
    int demodId = 0;
    float audioSampRate = 48000;
    Demodulator* currentDemod = NULL;

    VFOManager::VFO* vfo;

    WFMDemodulator wfmDemod;
    FMDemodulator fmDemod;
    AMDemodulator amDemod;
    USBDemodulator usbDemod;
    LSBDemodulator lsbDemod;
    DSBDemodulator dsbDemod;
    RAWDemodulator rawDemod;
    CWDemodulator cwDemod;

    dsp::NullSink<dsp::complex_t> ns;

    Event<float>::EventHandler srChangeHandler;
    SinkManager::Stream stream;

};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(ROOT_DIR "/radio_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RadioModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (RadioModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}