#include "SepiaEffect.h"

void SepiaEffect::Init(unsigned width, unsigned height)
{
	int index = int(m_buffers.size());
	m_buffers.push_back(new Framebuffer());
	m_buffers[index]->AddColorTarget(GL_RGBA8);
	m_buffers[index]->AddDepthTarget();
	m_buffers[index]->Init(width, height);

	//Set up shaders
	index = int(m_shaders.size());
	m_shaders.push_back(Shader::Create());
	m_shaders[index]->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
	m_shaders[index]->LoadShaderPartFromFile("shaders/Post/sepia_frag.glsl", GL_FRAGMENT_SHADER);
	m_shaders[index]->Link();
}

void SepiaEffect::ApplyEffect(PostEffect* buffer)
{
	BindShader(0);
	m_shaders[0]->SetUniform("u_Intensity", _intensity);

	buffer->BindColorAsTexture(0, 0, 0);

	m_buffers[0]->RenderToFSQ();

	buffer->UnbindTexture(0);

	UnbindShader();
}

float SepiaEffect::GetIntensity() const
{
	return _intensity;
}

void SepiaEffect::SetIntensity(float intensity)
{
	_intensity = intensity;
}
