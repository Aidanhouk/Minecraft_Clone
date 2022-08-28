//#define _CRT_SECURE_NO_WARNINGS
//#include <ctime>
#include "SkyManager.h"

#include "../Camera.h"
#include "../GlobalInfo.h"
#include "Player/PlayerInfo.h"
#include "Audio/SoundMaster.h"
#include "GlobalInfo.h"

#include <sstream>
#include <iostream>

//#define RealTime

#define degreesToRadians(x) x*(3.141592f/180.0f)

namespace {
    const unsigned int
        TIME_SUNRISE = 0u,
        TIME_DAY = 6000u,
        TIME_SUNSET = 12000u,
        TIME_NIGHT = 18000u;

    void playMusic(unsigned int dayTime);
    inline bool isInRange(int n, int x, int y);

    void printTime(std::ostringstream& stream, int time);
}

float blend(float x, float y, float factor)
{
    return x + ((y-x) * factor);
}

SkyManager::SkyManager()
	: sun("Environment/Sun"), moon("Environment/Moon")
{
	precipitationLightLevel = 1.0f;
	dayTime = 0; // Dawn
	m_prevTime = 0; // Time now
	m_currentPrecipitation = PrecipitationType::NONE;
	m_precipitationVisibility = 0.0f;

	const std::vector<GLfloat> sVertexCoords{
		-25,  25, 400,
		 25,  25, 400,
		 25, -25, 400,
		-25, -25, 400
	};

    const std::vector<GLfloat> sTextureCoords{
		0, 1,
		1, 1,
		1, 0,
		0, 0,
	};

    const std::vector<GLuint> sIndexCoords{
		0, 1, 2,
		2, 3, 0
	};

	m_SunModel.addData({ sVertexCoords, sTextureCoords, sIndexCoords });

    const std::vector<GLfloat> mVertexCoords{
		-15,  15, -400,
		 15,  15, -400,
		 15, -15, -400,
		-15, -15, -400
	};

    const std::vector<GLfloat> mTextureCoords{
		0, 1,
		1, 1,
		1, 0,
		0, 0,
	};

    const std::vector<GLuint> mIndexCoords{
		0, 1, 2,
		2, 3, 0
	};

	m_MoonModel.addData({ mVertexCoords, mTextureCoords, mIndexCoords });
}

void SkyManager::tickUpdate(unsigned int tickTime)
{
#ifdef RealTime
    struct tm newtime;
    time_t now = time(nullptr);
    localtime_s(&newtime, &now);
    int Month = 1 + newtime.tm_mon;
    dayTime = 1000 * newtime.tm_hour +
        (1000.0f / 60) * newtime.tm_min +
        (1000.0f / 60 / 60) * newtime.tm_sec;
    dayTime -= 6000;
    if (dayTime < 0)
        dayTime = 24000 - dayTime;
#else
    dayTime += tickTime - m_prevTime;
    m_prevTime = tickTime;
#endif
	playMusic(dayTime);

    if (!sf::Keyboard::isKeyPressed(sf::Keyboard::LControl)) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::PageUp)) {
            dayTime += 500;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::PageDown)) {
            dayTime += 23500;
            if (dayTime > 24000) {
                dayTime -= 24000;
            }
        }
    }

	if (dayTime > 23999)
		dayTime = 0;

	//Ambient light
	// 0 = 6 am
	// 6000 = 12 am
	// 12000 = 6 pm
	// 18000 = 12 pm
	if (dayTime < 1500) //6am - 9am sun gets brighter
        g_Info.lighting = blend(0.6f, 1.0f, (float)dayTime / 1500);
	else if (dayTime > 1500 && dayTime < 10500) //9am - 3pm sun is brightest
		g_Info.lighting = 1.0f;
	else if (dayTime > 10500 && dayTime < 12000) //3pm - 6pm sun gets dimmer
		g_Info.lighting = blend(1.0f, 0.6f, (float)((float)dayTime - 10500) / 1500);
    else if (dayTime > 12000 && dayTime < 13500) //6pm - 9pm sun light fades
		g_Info.lighting = blend(0.6f, 0.1f, (float)((float)dayTime - 12000) / 1500);
    else if (dayTime > 13500 && dayTime < 22500) //9pm - 3am is night
		g_Info.lighting = 0.1f;
    else if (dayTime > 22500 && dayTime < 24000)
		g_Info.lighting = blend(0.1f, 0.6f, (float)((float)dayTime - 22500) / 1500);

	g_Info.lighting *= precipitationLightLevel;

	//Update Sun/Moon matrix
	transformMatrix = glm::translate(glm::mat4(1.0f), playerPos);

	g_Info.dayTime = dayTime;
}

void SkyManager::setTime(unsigned int tickTime)
{
	dayTime = tickTime;
}

unsigned int SkyManager::getTime()
{
	return dayTime;
}

void SkyManager::update(glm::vec3 position, PrecipitationType precipitaionType)
{
	playerPos = position;

	static PrecipitationType nextPrecipitation = PrecipitationType::NONE;
	static int visibilityDirection = 1;
	static sf::Clock timer;

	if (g_PlayerInfo.underwater) {
		m_currentPrecipitation = PrecipitationType::NONE;
		m_precipitationVisibility = 0.0f;
		visibilityDirection = -1;

		g_SoundMaster.rainSetVolume(0);
	}
	else {
		g_SoundMaster.rainSetVolume(m_precipitationVisibility * 50.0f);
	}

	if (precipitaionType != nextPrecipitation && visibilityDirection != -1) {
		nextPrecipitation = precipitaionType;
		visibilityDirection *= -1;
	}

	if (timer.getElapsedTime().asSeconds() > 0.1f) {
		timer.restart();
		m_precipitationVisibility += visibilityDirection * 0.05f;

		if (!g_PlayerInfo.underwater) {
			switch (m_currentPrecipitation)
			{
			case PrecipitationType::RAIN:
			case PrecipitationType::SNOW:
				precipitationLightLevel -= visibilityDirection * 0.01f;
				if (precipitationLightLevel <= 0.7f) {
					precipitationLightLevel = 0.7f;
				}
				break;
			case PrecipitationType::NONE:
				precipitationLightLevel += visibilityDirection * 0.01f;
				if (precipitationLightLevel >= 1.0f) {
					precipitationLightLevel = 1.0f;
				}
				break;
			default:
				break;
			}
		}

		if (m_precipitationVisibility > 1.0f) {
			m_precipitationVisibility = 1.0f;
		}
		else if (m_precipitationVisibility <= 0.0f) {
			m_precipitationVisibility -= visibilityDirection * 0.05f;
			m_currentPrecipitation = nextPrecipitation;
			visibilityDirection *= -1;

			switch (m_currentPrecipitation)
			{
			case PrecipitationType::RAIN:
				g_SoundMaster.rainStart();
				break;
			case PrecipitationType::NONE:
			case PrecipitationType::SNOW:
				g_SoundMaster.rainStop();
				break;
			default:
				break;
			}
		}
	}
}

void SkyManager::renderSkyBox(const Camera& camera)
{
	//glDisable(GL_CULL_FACE);
	skyBox.render(camera, precipitationLightLevel);
}

void SkyManager::render(const Camera& camera)
{
	if (!g_Info.fog) {
		skyBox.render(camera, precipitationLightLevel);
	}

	// Sun render
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	m_shader.useProgram();
	m_SunModel.bindVAO();
	sun.bindTexture();

	m_shader.loadModelMatrix(transformMatrix);
	m_shader.loadProjectionViewMatrix(camera.getProjectionViewMatrix());
	m_shader.loadTime(degreesToRadians(((float)dayTime / 24000) * 360));
	GL::drawElements(m_SunModel.getIndicesCount());

	// Moon render
	//m_shader.useProgram();
	m_MoonModel.bindVAO();
	moon.bindTexture();

	m_shader.loadModelMatrix(transformMatrix);
	m_shader.loadProjectionViewMatrix(camera.getProjectionViewMatrix());
	m_shader.loadTime(degreesToRadians(((float)dayTime / 24000) * 360));
	GL::drawElements(m_MoonModel.getIndicesCount());

	clouds.Render(camera, playerPos);

	switch (m_currentPrecipitation)
	{
	case PrecipitationType::RAIN:
		rain.Render(camera, playerPos, m_precipitationVisibility);
		break;
	case PrecipitationType::SNOW:
		snowfall.Render(camera, playerPos, m_precipitationVisibility);
		break;
	case PrecipitationType::NONE:
	default:
		break;
	}

	g_Info.cam = (Camera*)&camera;
}

void SkyManager::printDebugInfo(std::ostringstream& stream)
{
    stream
        << "\n"
        << "\n <F4>   Fog - " << g_Info.fog
        << "\n <F5>   Weather - " << g_Info.weather
        << "\n <P>    Post processing - " << g_Config.postProcess
        << "\n"
        ;

    if (g_Info.dayTime >= 18000) {
        int time = g_Info.dayTime - 18000;
        printTime(stream, time);
        stream << " AM";
    }
    else if (g_Info.dayTime < 6000) {
        int time = g_Info.dayTime + 6000;
        printTime(stream, time);
        stream << " AM";
    }
    else {
        int time = g_Info.dayTime - 6000;
        printTime(stream, time);
        stream << " PM";
    }
}

namespace {
    void playMusic(unsigned int dayTime)
    {
        bool play = false;
        if (isInRange(dayTime, TIME_SUNRISE, TIME_SUNRISE + 100u))
            g_SoundMaster.keepPlaying(MusicSet::DefaultInGameSet);

        else if (isInRange(dayTime, TIME_DAY, TIME_DAY + 100u))
            g_SoundMaster.keepPlaying(MusicSet::DefaultInGameSet);

        else if (isInRange(dayTime, TIME_SUNSET, TIME_SUNSET + 100u))
            g_SoundMaster.keepPlaying(MusicSet::DefaultInGameSet);

        else if (isInRange(dayTime, TIME_NIGHT, TIME_NIGHT + 100u))
            g_SoundMaster.keepPlaying(MusicSet::DefaultInGameSet);
    }

    inline bool isInRange(int n, int x, int y)
    {
        return n >= x && n < y;
    }

    void printTime(std::ostringstream& stream, int time)
    {
        int hours = time / 1000;
        std::string hourZero = hours < 10 ? "0" : "";
        int minutes = (time % 1000) / 1000.0f * 60;
        std::string minutesZero = minutes < 10 ? "0" : "";
        stream
            << "\n <PageUp/PageDown>"
            << "\n Time: " << hourZero << hours << ":" << minutesZero << minutes
            ;
    }
}
