// Big file, I'm lazy

#include "main.hpp"

#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/Graphics.hpp"
#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/ImageConversion.hpp"
#include "UnityEngine/SpriteMeshType.hpp"
#include "UnityEngine/Vector4.hpp"

#include "GlobalNamespace/CachedMediaAsyncLoader.hpp"
#include "GlobalNamespace/AsyncCachedLoader_2.hpp"
#include "GlobalNamespace/MediaAsyncLoader.hpp"

#include "System/Threading/Tasks/Task.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "System/Runtime/CompilerServices/YieldAwaitable.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Func_3.hpp"

#include "custom-types/shared/delegate.hpp"

#include <filesystem>
#include <fstream>

using namespace UnityEngine;
using namespace UnityEngine::Networking;
using namespace GlobalNamespace;

using namespace System::Threading::Tasks;

constexpr auto IMAGE_SIZE = 256;
static ModInfo modInfo; // Stores the ID and version of our mod, and is sent to the modloader upon startup

// Loads the config from disk using our modInfo, then returns it for use
// other config tools such as config-utils don't use this config, so it can be removed if those are in use
Configuration &getConfig() {
    static Configuration config(modInfo);
    return config;
}

// Returns a logger, useful for printing debug messages
Logger &getLoggerOld() {
    static Logger *logger = new Logger(modInfo);
    return *logger;
}

// Called at the early stages of game loading
extern "C" void setup(ModInfo &info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;

    getConfig().Load();
    Paper::Logger::RegisterFileContextId(PLogger.tag);
    PLogger.fmtLog<Paper::LogLevel::INF>("Completed setup!");
}

#pragma region BSML

// Stolen from BSML and improved upon
struct ScaleOptions {
    int width;
    int height;
    bool maintainRatio;
    bool shouldScale;

    ScaleOptions(int width = 0, int height = 0, bool maintainRatio = true, bool shouldScale = false)
            : width(width), height(height), maintainRatio(maintainRatio), shouldScale(shouldScale) {}
};


Texture2D *DownScaleTexture(Texture2D *tex, const ScaleOptions &options) {
    auto originalWidth = tex->get_width();
    auto originalHeight = tex->get_height();

    if (originalWidth + originalHeight <= options.width + options.height)
        return tex;

    auto newWidth = options.width;
    auto newHeight = options.height;
    if (options.maintainRatio) {
        auto ratio = (float) originalWidth / (float) originalHeight;
        auto scale = originalWidth > originalHeight ? originalWidth : originalHeight;

        if (scale * ratio <= originalWidth) {
            originalWidth = scale * ratio;
            originalHeight = scale;
        } else {
            originalWidth = scale;
            originalHeight = scale * ratio;
        }
    }

    auto currentRT = RenderTexture::get_active();

    auto rect = Rect(0, 0, newWidth, newHeight);
    auto copy = Texture2D::New_ctor(rect.get_width(), rect.get_height(), TextureFormat::RGBA32, false);
    auto renderTexture = RenderTexture::GetTemporary(rect.get_width(), rect.get_height(), 32,
                                                     RenderTextureFormat::Default, RenderTextureReadWrite::Default);
    Graphics::Blit(tex, renderTexture);

//    RenderTexture::set_active(renderTexture);
    Graphics::CopyTexture(renderTexture, 0, 0, copy, 0, 0);
    RenderTexture::ReleaseTemporary(renderTexture);
    RenderTexture::set_active(currentRT);


    return copy;
}

#pragma endregion

Texture2D *ReadAndResizeImageAsync(
        StringW path,
        System::Threading::CancellationToken cancellationToken
) {
    if (cancellationToken.get_IsCancellationRequested()) {
        return nullptr;
    }

    std::string sysPath = path;

    std::ifstream file = std::ifstream(sysPath.c_str(), std::ios::binary);

    // https://stackoverflow.com/questions/15138353/how-to-read-a-binary-file-into-a-vector-of-unsigned-chars

    // Stop eating new lines in binary mode!!!
    file.unsetf(std::ios::skipws);


    if (!file.is_open()) {
        return nullptr;
    }

    // get its size:
    std::streampos fileSize;

    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // reserve capacity
    ArrayW<uint8_t> data(fileSize);

    std::thread s([&cancellationToken, &file, &data] {
        if (cancellationToken.get_IsCancellationRequested()) {
            return;
        }

        // read the data:
        std::copy(std::istream_iterator<uint8_t>(file),
                  std::istream_iterator<uint8_t>(),
                //dest
                  data.begin());
    });

    while (s.joinable()) {
        if (cancellationToken.get_IsCancellationRequested()) {
            return nullptr;
        }

        Task::Yield();
    }

    if (cancellationToken.get_IsCancellationRequested()) {
        return nullptr;
    }

    auto spriteTexture = Texture2D::New_ctor(IMAGE_SIZE, IMAGE_SIZE);

    if (cancellationToken.get_IsCancellationRequested()) {
        return nullptr;
    }

    if (!ImageConversion::LoadImage(spriteTexture, data, true))
        return nullptr;

    if (cancellationToken.get_IsCancellationRequested()) {
        return nullptr;
    }

    if (spriteTexture->get_width() > IMAGE_SIZE || spriteTexture->get_width() > IMAGE_SIZE) {
        auto oldSpriteTexture = spriteTexture;
        spriteTexture = DownScaleTexture(oldSpriteTexture, ScaleOptions(IMAGE_SIZE, IMAGE_SIZE));
        Object::Destroy(oldSpriteTexture);
    }

    return spriteTexture;
}

System::Threading::Tasks::Task_1<Sprite *> *MediaAsyncLoader_LoadSpriteAsync_replacement(
        StringW path,
        System::Threading::CancellationToken cancellationToken
) {
    Paper::Profiler profiler;
    profiler.startTimer();


    if (!std::filesystem::exists(path.operator std::string())) {

        auto sp = MediaAsyncLoader::LoadSpriteAsync(path, cancellationToken);

//        while (!sp->get_IsCompleted()) {
//            Task::Yield();
//        }
//
//        profiler.mark("Sprite read");


//    std::function<Sprite*(Task_1<Sprite*>*)> f = [](Task_1<Sprite*>* sp) -> Sprite* {
//
//    };
//
//
//    return sp->ContinueWith<Sprite*>(custom_types::MakeDelegate<AsyncFunc2<Sprite*>>(f));

//    return sp;
//}
        return sp;
    }


    auto spriteTexture = ReadAndResizeImageAsync(path, cancellationToken);


    auto spriteTextureResult = spriteTexture;
    float pixelsPerUnit = 100.0f;
    auto result = spriteTexture != nullptr
                  ? Sprite::Create(
                    spriteTextureResult,
                    Rect(0, 0, spriteTextureResult->get_width(), spriteTextureResult->get_height()),
                    Vector2(0, 0),
                    pixelsPerUnit,

                    0,
                    SpriteMeshType::Tight,
                    Vector4(0, 0, 0, 0),
                    false
            )
                  : nullptr;

    profiler.endTimer();
    profiler.printMarks(PLogger.tag);

    return Task_1<Sprite *>::New_ctor(result);
}


MAKE_HOOK_MATCH(LoadSpriteAsyncPatch, &CachedMediaAsyncLoader::LoadSpriteAsync,
                ::System::Threading::Tasks::Task_1<::UnityEngine::Sprite *>*, CachedMediaAsyncLoader *self,
                StringW path,
                ::System::Threading::CancellationToken cancellationToken) {
    if (self->spriteAsyncCachedLoader == nullptr) {
        std::function<System::Threading::Tasks::Task_1<Sprite *> *(StringW,
                                                                   ::System::Threading::CancellationToken)> f = MediaAsyncLoader_LoadSpriteAsync_replacement;

        static SafePtr<System::Func_3<StringW, ::System::Threading::CancellationToken, ::System::Threading::Tasks::Task_1<Sprite *> *>> delegate =
                custom_types::MakeDelegate<
                        System::Func_3<
                                StringW,
                                ::System::Threading::CancellationToken,
                                ::System::Threading::Tasks::Task_1<Sprite *> *
                        >
                        *>(f);

        self->spriteAsyncCachedLoader = AsyncCachedLoader_2<StringW, Sprite *>::New_ctor(
                self->maxNumberOfSpriteCachedElements,
                delegate.ptr()
        );
        PLogger.fmtLog<Paper::LogLevel::INF>("replaced LoadSpriteAsync");
    }

    return LoadSpriteAsyncPatch(self, path, cancellationToken);
}


// Called later on in the game loading - a good time to install function hooks
extern "C" void load() {
    il2cpp_functions::Init();

    PLogger.fmtLog<Paper::LogLevel::INF>("Installing hooks...");
    INSTALL_HOOK(getLoggerOld(), LoadSpriteAsyncPatch);
    PLogger.fmtLog<Paper::LogLevel::INF>("Installed all hooks!");
}