#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <fmod.hpp>
#include <algorithm>
#include <iomanip>

using namespace geode::prelude;


float g_zoomMax = 1.0f;
float g_bassIntensity = 0.9f;
float g_shakeIntensity = 12.0f;
bool g_hideMenuButtons = false;

class MySettingsLayer : public FLAlertLayer {
    TextInput* m_zoomInput;
    TextInput* m_intensityInput;
    TextInput* m_shakeInput;

public:
    static MySettingsLayer* create() {
        auto ret = new MySettingsLayer();
        if (ret && ret->init(150)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init(int bgOpacity) {
        if (!FLAlertLayer::init(bgOpacity)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        auto bg = CCScale9Sprite::create("GJ_square04.png");
        bg->setContentSize({ 280, 240 });
        bg->setPosition(winSize / 2);
        m_mainLayer->addChild(bg);

        m_buttonMenu = CCMenu::create();
        m_mainLayer->addChild(m_buttonMenu);

        auto title = CCLabelBMFont::create("Settings", "goldFont.fnt");
        title->setPosition({ winSize.width / 2, winSize.height / 2 + 100 });
        title->setScale(0.7f);
        m_mainLayer->addChild(title);

        g_zoomMax = Mod::get()->getSavedValue<float>("save_zoom", 1.0f);
        g_bassIntensity = Mod::get()->getSavedValue<float>("save_intensity", 0.9f);
        g_shakeIntensity = Mod::get()->getSavedValue<float>("save_shake", 12.0f);

        m_zoomInput = createInput("Zoom", 60, g_zoomMax, "save_zoom", &g_zoomMax);
        m_intensityInput = createInput("Intensity", 0, g_bassIntensity, "save_intensity", &g_bassIntensity);
        m_shakeInput = createInput("Shake", -60, g_shakeIntensity, "save_shake", &g_shakeIntensity);

        auto closeBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"),
            this, menu_selector(MySettingsLayer::onClose));
        closeBtn->setPosition({ -130, 110 });
        m_buttonMenu->addChild(closeBtn);

     
        auto infoBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png"),
            this, menu_selector(MySettingsLayer::onInfo));
        infoBtn->setPosition({ 130, 110 });
        m_buttonMenu->addChild(infoBtn);

        this->setTouchEnabled(true);
        this->setKeypadEnabled(true);
        return true;
    }

    TextInput* createInput(const char* labelStr, float y, float initialVal, std::string saveKey, float* globalVar) {
        auto label = CCLabelBMFont::create(labelStr, "bigFont.fnt");
        label->setScale(0.4f);
        label->setPosition({0, y + 25});
        m_buttonMenu->addChild(label);

        auto input = TextInput::create(100.f, labelStr, "chatFont.fnt");
        input->setFilter("0123456789.");
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << initialVal;
        input->setString(ss.str());
        
        input->setPosition({0, y});
        
        input->setCallback([saveKey, globalVar](const std::string& text) {
            if (text.empty()) return;
            try {
                float val = std::stof(text);
                *globalVar = val;
                Mod::get()->setSavedValue(saveKey, val);
            } catch(...) {}
        });

        m_buttonMenu->addChild(input);
        return input;
    }

    void onInfo(CCObject*) {
        FLAlertLayer::create(
            "Help",
            "<cy>Original Code by:</c> <cr>thesillydoggo</c> and <cp>EryManthus</c> luv for them <3!\n\n"
            "<cg>Zoom:</c> How much the background scales with music.\n"
            "<cg>Intensity:</c> Bass sensitivity.\n"
            "<cg>Shake:</c> Background vibration.\n\n"
            "<cy>PC:</c> Type with keyboard | <cg>Mobile:</c> Tap to type.",
            "OK"
        )->show();
    }
    
    void onClose(CCObject*) { this->removeFromParentAndCleanup(true); }
    void keyBackClicked() override { onClose(nullptr); }
};

class BGPulsingNode : public CCNode {
public:
    CCSprite* bg = nullptr;
    FMOD::DSP* fftDSP = nullptr;
    float smoothBass = 0.0f;
    float baseScale = 1.0f;
    CCPoint basePos;

    static BGPulsingNode* create(CCSprite* bg) {
        auto ret = new BGPulsingNode();
        if (ret && ret->init(bg)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init(CCSprite* target) {
        if (!CCNode::init()) return false;
        bg = target;
        baseScale = bg->getScale();
        basePos = bg->getPosition();

        auto engine = FMODAudioEngine::sharedEngine();
        auto sys = engine->m_system;
        FMOD::ChannelGroup* master = nullptr;
        sys->getMasterChannelGroup(&master);
        sys->createDSPByType(FMOD_DSP_TYPE_FFT, &fftDSP);
        fftDSP->setParameterInt(FMOD_DSP_FFT_WINDOWSIZE, 512);
        master->addDSP(0, fftDSP);

        scheduleUpdate();
        return true;
    }

    void update(float dt) override {
        if (!fftDSP || !bg) return;
        
        FMOD_DSP_PARAMETER_FFT* fft = nullptr;
        fftDSP->getParameterData(FMOD_DSP_FFT_SPECTRUMDATA, (void**)&fft, nullptr, nullptr, 0);

        float bass = 0.0f;
        if (fft && fft->numchannels > 0 && fft->spectrum[0]) {
            for (int i = 0; i < 8; i++) bass += fft->spectrum[0][i];
            bass /= 8;
        }
        smoothBass += (bass - smoothBass) * dt * 14.0f;

        float currentBassValue = smoothBass * g_bassIntensity;
        bg->setScale(baseScale * (1.0f + (currentBassValue * g_zoomMax)));
        
        float shake = currentBassValue * g_shakeIntensity;
        bg->setPosition({ 
            basePos.x + CCRANDOM_MINUS1_1() * shake, 
            basePos.y + CCRANDOM_MINUS1_1() * shake 
        });
    }

    ~BGPulsingNode() { if (fftDSP) fftDSP->release(); }
};

class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        g_zoomMax = Mod::get()->getSavedValue<float>("save_zoom", 1.0f);
        g_bassIntensity = Mod::get()->getSavedValue<float>("save_intensity", 0.9f);
        g_shakeIntensity = Mod::get()->getSavedValue<float>("save_shake", 12.0f);

        auto bg = static_cast<CCSprite*>(this->getChildByID("main-menu-bg"));
        if (bg) this->addChild(BGPulsingNode::create(bg), -1);

        if (auto bottomMenu = this->getChildByID("bottom-menu")) {
            auto sprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");
            auto btn = CCMenuItemSpriteExtra::create(
                sprite, this, menu_selector(MyMenuLayer::onCustomSettings)
            );
            bottomMenu->addChild(btn);

            // Botón extra para alternar visibilidad de los botones del menú
            auto hideSprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
            auto hideBtn = CCMenuItemSpriteExtra::create(
                hideSprite, this, menu_selector(MyMenuLayer::onToggleHideButtons)
            );
            hideBtn->setID("invisible-toggle-btn"_spr);
            bottomMenu->addChild(hideBtn);

            bottomMenu->updateLayout();
        }

        if (g_hideMenuButtons) {
            applyInvisibleState(true);
        }

        return true;
    }

    void onCustomSettings(CCObject* sender) {
        MySettingsLayer::create()->show();
    }

    void onToggleHideButtons(CCObject* sender) {
        g_hideMenuButtons = !g_hideMenuButtons;
        applyInvisibleState(g_hideMenuButtons);
    }

    void applyInvisibleState(bool hide) {
        const char* menuNames[] = { "bottom-menu", "right-menu", "side-menu", "player-menu", "main-menu" };
        
        for (const char* name : menuNames) {
            if (auto menu = this->getChildByID(name)) {
                auto children = menu->getChildren();
                if (children) {
                    for (int i = 0; i < children->count(); i++) {
                        if (auto node = typeinfo_cast<CCNode*>(children->objectAtIndex(i))) {
                            if (node->getID() == "invisible-toggle-btn"_spr) continue;
                            
                            // Opacidad a 0 los vuelve invisibles pero preserva el área táctil intacta
                            node->setOpacity(hide ? 0 : 255);
                        }
                    }
                }
            }
        }
    }
};
