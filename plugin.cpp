#include "Manager.h"

LightSourceManager* LSM;

class OurEventSink : public RE::BSTEventSink<RE::TESEquipEvent> {
    OurEventSink() = default;
    OurEventSink(const OurEventSink&) = delete;
    OurEventSink(OurEventSink&&) = delete;
    OurEventSink& operator=(const OurEventSink&) = delete;
    OurEventSink& operator=(OurEventSink&&) = delete;

public:
    static OurEventSink* GetSingleton() {
        static OurEventSink singleton;
        return &singleton;
    }
    
    RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!LSM->allow_equip_event_sink) return RE::BSEventNotifyControl::kContinue;
        if (!LSM->IsValidSource(event->baseObject)) return RE::BSEventNotifyControl::kContinue;
        if (event->equipped) {
            if (!LSM->SetSource(event->baseObject)) logger::info("Failed to set source. Something is terribly wrong!!!");
            logger::info("{} equipped.", LSM->GetName());
            LSM->StartBurn();
		}
        else {
            logger::info("{} unequipped.", LSM->GetName());
			LSM->StopBurn();
            logger::info("timer stopped.");
		}
        return RE::BSEventNotifyControl::kContinue;
    }


};

void OnMessage(SKSE::MessagingInterface::Message* message) {
    switch (message->type) {
        case SKSE::MessagingInterface::kPostPostLoad:
            logger::info("Postpostload.");
            RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESEquipEvent>(OurEventSink::GetSingleton());
            break;
        case SKSE::MessagingInterface::kNewGame:
            logger::info("Newgame.");
            LSM->Reset();
            logger::info("Newgame LSM reset succesful.");
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            logger::info("Preload.");
            LSM->Reset();
            logger::info("Preload LSM reset succesful.");
			break;
        case SKSE::MessagingInterface::kPostLoadGame:
            logger::info("Postload.");
            LSM->LogRemainings();
            if (!LSM->DetectSetSource()) logger::info("No source detected, i.e. player wasnt wearing any source.");
            logger::info("Postload LSM succesful.");
			break;
    }
};

void SaveCallback(SKSE::SerializationInterface* serializationInterface) {
    logger::info("Save Start");
    if (LSM->is_burning) LSM->PauseBurn();
    LSM->SendData();
    LSM->LogRemainings();
    if (!LSM->Save(serializationInterface, Settings::kDataKey, Settings::kSerializationVersion)) {
        logger::critical("Failed to save Data");
    }
    if (LSM->is_burning) LSM->StartBurn();
    logger::info("Data Saved");
}

void LoadCallback(SKSE::SerializationInterface* serializationInterface) {
    logger::info("Load Start");
    std::uint32_t type;
    std::uint32_t version;
    std::uint32_t length;
    logger::info("Load Start");
    while (serializationInterface->GetNextRecordInfo(type, version, length)) {
        auto temp = Utilities::DecodeTypeCode(type);
        logger::info("Trying Load for {}", temp);

        if (version != Settings::kSerializationVersion) {
            logger::critical("Loaded data has incorrect version. Recieved ({}) - Expected ({}) for Data Key ({})",
                version, Settings::kSerializationVersion,temp);
            continue;
        }
        switch (type) {
            case Settings::kDataKey: {
                if (!LSM->Load(serializationInterface)) {
                    logger::critical("Failed to Load Data");
                }
            } break;
            default:
                logger::critical("Unrecognized Record Type: {}", temp);
                break;
        }
    }
    LSM->ReceiveData();
    logger::info("Data loaded from skse co-save.");
}

void InitializeSerialization() {
    SKSE::log::trace("Initializing cosave serialization...");
    auto* serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(Settings::kDataKey);
    serialization->SetSaveCallback(SaveCallback);
    // serialization->SetRevertCallback(LightSourceManager::RevertCallback);
    serialization->SetLoadCallback(LoadCallback);
    SKSE::log::trace("Cosave serialization initialized.");
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    spdlog::set_pattern("%v");
    SKSE::Init(skse);
    auto sources = Settings::LoadSettings();
    LSM = LightSourceManager::GetSingleton(sources, 500);
    InitializeSerialization();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}