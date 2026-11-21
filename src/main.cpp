// TODO: Compatibility with Jukebox

#include <Geode/Geode.hpp>

using namespace geode::prelude;

// https://discord.com/channels/911701438269386882/979402752121765898/1441895974967705752
geode::Result<std::string_view> getSpriteNodeFrameName(cocos2d::CCSprite* sprite) {
    std::string_view frameName = "";
    // taken from devtools
    if (auto texture = sprite->getTexture()) {
        auto cachedFrames = cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache()->m_pSpriteFrames;
        auto rect = sprite->getTextureRect();
        for (auto [key, frame] : geode::cocos::CCDictionaryExt<std::string_view, cocos2d::CCSpriteFrame*>(cachedFrames)) {
            if (frame->getTexture() == texture && frame->getRect() == rect) {
                frameName = key;
                break;
            }
        }
    }

    if (frameName.empty()) {
        return geode::Err("Sprite does not have a frame name found in cache!");
    }

    return geode::Ok(frameName);
}

#include <Geode/modify/LevelSettingsLayer.hpp>
class $modify(HookedLSL, LevelSettingsLayer) {
	static HookedLSL* instance;

	bool init(LevelSettingsObject* object, LevelEditorLayer* layer) {
		if (LevelSettingsLayer::init(object, layer) == false) {
			return false;
		}
		log::debug("LSL::Init called");
		instance = this; // hope this workss

		return true;
	}

	void onClose(CCObject* sender) {
        if (instance == this) {
            instance = nullptr;
		}

		log::debug("Closing lSL");
        LevelSettingsLayer::onClose(sender);
    }

	float getSogDuration() {
		log::debug("lsl hi");
		auto label = typeinfo_cast<CCLabelBMFont*>(this->getChildByType<CCLayer>(0)->getChildByID("song-label"));
		if (!label) {
			return -1.0f;
		}
		std::string name = utils::string::trim(label->getString());
		log::debug("LSL: label: '{}', len {}", name, name.length());
		
		#include "bigsongiftable.inc"
	}
};
HookedLSL* HookedLSL::instance = nullptr;

#include <Geode/modify/CustomSongLayer.hpp>
class $modify(HookedCSL, CustomSongLayer) {
	static HookedCSL* instance;

	bool init(CustomSongDelegate* delegate) {
		if (CustomSongLayer::init(delegate) == false) {
			return false;
		}
		log::debug("CSL::Init called");
		instance = this; // hope this workss

		return true;
	}

	void onClose(CCObject* sender) {
        if (instance == this) {
            instance = nullptr;
		}

		log::debug("Closing CSL");
        CustomSongLayer::onClose(sender);
    }

	float getSogDuration() {
		log::debug("CSL: Scientits reveal the Hi");

		auto widget = this->getChildByType<CCLayer>(0)->getChildByID("CustomSongWidget");
		if (!widget) {
			log::debug("CSL: CustomSongWidget NF");
			return -1.0f;
		}

 		auto widgetl = typeinfo_cast<CCLabelBMFont*>(widget->getChildByID("id-and-size-label"));
		if (!widgetl) {
			log::debug("CSL: Label NF");
			return -1.0f;
		}

		std::string labelstr = widgetl->getString();

		if (labelstr.empty()) { // oh boy oh boy
			log::debug("CSL: will call LSL sogger");
			return -2.0f;
		}

		log::debug("CSL: labelstr: '{}'", labelstr);
		
		auto start = labelstr.find("SongID: ") + 8;
		auto end = labelstr.find(" ", start);

		int songID = std::stoi(labelstr.substr(start, end - start));
		std::string path = MusicDownloadManager::sharedState()->pathForSong(songID);
		log::debug("CSL: extracted: {}", songID);
		log::debug("CSL: extracted2: {}", path);

		// welcome to fmod hjell
		FMOD::Sound* sound = nullptr;
		auto fmodding = FMODAudioEngine::sharedEngine()->m_system->createSound(path.c_str(), FMOD_OPENONLY | FMOD_ACCURATETIME, nullptr, &sound);
		if (fmodding != FMOD_OK || !sound) {
			log::debug("we not fmodding, fmod gave error");
			return -1.0f;
		}

		unsigned int duration = 0;
		fmodding = sound->getLength(&duration, FMOD_TIMEUNIT_MS);
		sound->release();

		if (fmodding != FMOD_OK) {
			log::debug("we not fmodding further, fmod gave error");
			return -1.0f;
		}

		
		log::debug("FMOD returned duration {}", duration);
		return duration / 1000.0f;
	}
};
HookedCSL* HookedCSL::instance = nullptr;

#include <Geode/modify/SongOptionsLayer.hpp>
class $modify(BetterSOL, SongOptionsLayer) {
	struct Fields : TextInputDelegate {
		Slider* slidey = nullptr;
		CCTextInputNode* text = nullptr;
		CCLabelBMFont* helpme = nullptr;
		float maxDuration = 676.7f; // placeholder
		bool validDuration = false;

		void updateSongDuration() {
			log::debug("we ballin?");
			if (HookedCSL::instance) {
				log::debug("csl inst ok");
				float duration = HookedCSL::instance->getSogDuration();
				if (duration == -2.0f) { // main level song
					log::debug("csl sog returned invalid, IS main");
					if (HookedLSL::instance) {
						log::debug("lsl inst ok");
						duration = HookedLSL::instance->getSogDuration();
					}
				} else if (duration == -1.0f) { // customsongwidget doesnt exist?
					log::debug("csl sog returned invalid, non main");
				} else { // we got the duration
					log::debug("csl sog returned valid");
				}
				maxDuration = duration;
				validDuration = duration > 0.0f;
			}
		}

		void textChanged(CCTextInputNode* text) override {
			float seconds = 0.0f;

			if (maxDuration <= 0.0f || !slidey) {
				return;
			}

			if (text->getString().empty()) {
				slidey->setValue(0.0f);
				return;
			}

			seconds = std::clamp(std::stof(text->getString()), 0.0f, maxDuration);
			slidey->setValue(seconds / maxDuration);
		}
	};

	bool init(CustomSongDelegate* delegate) {
		if (SongOptionsLayer::init(delegate) == false) {
			return false;
		}
		log::debug("SOL::Init called");

		auto layer = this->getChildByType<CCLayer>(0);
		if (!layer) {
			return true;
		}

		auto text = layer->getChildByType<CCTextInputNode>(0);
		if (!text) {
			return true;
		}

		auto fields = m_fields.self();
		fields->text = text;
		fields->updateSongDuration();
		log::debug("Got song duration: {}", fields->maxDuration);

		auto death = CCLabelBMFont::create(fmt::format("/ {:.2f}", fields->maxDuration).c_str(), "bigFont.fnt");
		death->setScale(0.35f);
		death->setAnchorPoint({0.0f, 0.5f});
		death->setPosition(text->getPosition() + CCPoint{ text->getContentSize().width / 2 + 3.0f, 0.0f });

		layer->addChild(death);
		fields->helpme = death;
		death->setVisible(fields->validDuration);

		auto slidey = Slider::create(this, menu_selector(BetterSOL::onSliderChanged), 1.0f);
		if (!slidey) {
			return true;
		}
		slidey->setID("offset-slider"_spr);

		slidey->setPosition({layer->getContentSize().width/2, 150.0f});
		layer->addChild(slidey);
		fields->slidey = slidey;
		slidey->setVisible(fields->validDuration);

		text->setDelegate(fields);
		fields->textChanged(text);

		// node ids, where be
		auto menu = layer->getChildByType<CCMenu>(0);
		if (menu) {
			for (CCNode* node : menu->getChildrenExt()) {
				auto item = typeinfo_cast<CCMenuItemSpriteExtra*>(node);
				if (!item) {
					continue;
				}

				auto sprite = typeinfo_cast<CCSprite*>(item->getNormalImage());
				if (!sprite) {
					continue;
				}

				auto fname = getSpriteNodeFrameName(sprite);
				if (!fname.isOk()) {
					continue;
				}

				if (*fname == "GJ_playMusicBtn_001.png") {
					item->setPositionX(item->getPositionX() + 20.0f);
					break;
				}
			}
		}

		return true;
	}

	void onSliderChanged(CCObject* sender) {
		auto fields = m_fields.self();

		if (!fields->slidey || !fields->text || fields->maxDuration <= 0.0f) {
			return;
		}

		float seconds = std::round(fields->slidey->getValue() * fields->maxDuration * 10.0f) / 10.0f;
		fields->text->setString(fmt::format("{:.1f}", seconds).c_str());
	}
};