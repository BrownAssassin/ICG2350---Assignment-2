//Just a simple handler for simple initialization stuffs
#include "Utilities/BackendHandler.h"

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <Texture2D.h>
#include <Texture2DData.h>
#include <MeshBuilder.h>
#include <MeshFactory.h>
#include <NotObjLoader.h>
#include <ObjLoader.h>
#include <VertexTypes.h>
#include <ShaderMaterial.h>
#include <RendererComponent.h>
#include <TextureCubeMap.h>
#include <TextureCubeMapData.h>

#include <Timing.h>
#include <GameObjectTag.h>
#include <InputHelpers.h>

#include <IBehaviour.h>
#include <CameraControlBehaviour.h>
#include <FollowPathBehaviour.h>
#include <SimpleMoveBehaviour.h>

bool lightState = true;
bool defaultCube = true;

int main() {
	int frameIx = 0;
	float fpsBuffer[128];
	float minFps, maxFps, avgFps;
	int selectedVao = 0; // select cube by default
	std::vector<GameObject> controllables;

	BackendHandler::InitAll();

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(BackendHandler::GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Push another scope so most memory should be freed *before* we exit the app
	{
		#pragma region Shader and ImGui
		Shader::sptr passthroughShader = Shader::Create();
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_frag.glsl", GL_FRAGMENT_SHADER);
		passthroughShader->Link();

		Shader::sptr colorCorrectionShader = Shader::Create();
		colorCorrectionShader->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
		colorCorrectionShader->LoadShaderPartFromFile("shaders/Post/color_correction_frag.glsl", GL_FRAGMENT_SHADER);
		colorCorrectionShader->Link();

		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_phong.glsl", GL_FRAGMENT_SHADER);
		shader->Link();

		Shader::sptr sinShader = Shader::Create();
		sinShader->LoadShaderPartFromFile("shaders/vertex_sin.glsl", GL_VERTEX_SHADER);
		sinShader->LoadShaderPartFromFile("shaders/frag_phong.glsl", GL_FRAGMENT_SHADER);
		sinShader->Link();

		float	  effectState = 0.0;
		glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 10.0f);
		glm::vec3 lightCol = glm::vec3(0.9f, 0.85f, 0.5f);
		float     lightAmbientPow = 0.05f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.1f;
		float     lightLinearFalloff = 0.09f;
		float     lightQuadraticFalloff = 0.032f;

		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader->SetUniform("u_LightPos", lightPos);
		shader->SetUniform("u_LightCol", lightCol);
		shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		shader->SetUniform("u_AmbientCol", ambientCol);
		shader->SetUniform("u_AmbientStrength", ambientPow);
		shader->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);

		sinShader->SetUniform("effectState", effectState);
		sinShader->SetUniform("u_LightPos", lightPos);
		sinShader->SetUniform("u_LightCol", lightCol);
		sinShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		sinShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		sinShader->SetUniform("u_AmbientCol", ambientCol);
		sinShader->SetUniform("u_AmbientStrength", ambientPow);
		sinShader->SetUniform("u_LightAttenuationConstant", 1.0f);
		sinShader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		sinShader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);

		// We'll add some ImGui controls to control our shader
		BackendHandler::imGuiCallbacks.push_back([&]() {
			if (ImGui::CollapsingHeader("Environment generation"))
			{
				if (ImGui::Button("Regenerate Environment", ImVec2(200.0f, 40.0f)))
				{
					EnvironmentGenerator::RegenerateEnvironment();
				}
			}
			if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
					shader->SetUniform("u_LightPos", lightPos);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}

			auto name = controllables[selectedVao].get<GameObjectTag>().Name;
			ImGui::Text(name.c_str());
			auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
			ImGui::Checkbox("Relative Rotation", &behaviour->Relative);

			ImGui::Text("Q/E -> Yaw\nLeft/Right -> Roll\nUp/Down -> Pitch\nY -> Toggle Mode");
		
			minFps = FLT_MAX;
			maxFps = 0;
			avgFps = 0;
			for (int ix = 0; ix < 128; ix++) {
				if (fpsBuffer[ix] < minFps) { minFps = fpsBuffer[ix]; }
				if (fpsBuffer[ix] > maxFps) { maxFps = fpsBuffer[ix]; }
				avgFps += fpsBuffer[ix];
			}
			ImGui::PlotLines("FPS", fpsBuffer, 128);
			ImGui::Text("MIN: %f MAX: %f AVG: %f", minFps, maxFps, avgFps / 128.0f);
			});

		#pragma endregion 

		// GL states
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL); // New 

		#pragma region TEXTURE LOADING

		// Load some textures from files
		Texture2D::sptr sandStone = Texture2D::LoadFromFile("images/Stone_001_Diffuse.png");
		Texture2D::sptr sandStoneSpec = Texture2D::LoadFromFile("images/Stone_001_Specular.png");
		Texture2D::sptr stone = Texture2D::LoadFromFile("images/stone.jpg");
		Texture2D::sptr stoneSpec = Texture2D::LoadFromFile("images/stoneSpec.jpg");
		Texture2D::sptr grass = Texture2D::LoadFromFile("images/grass.jpg");
		Texture2D::sptr grassSpec = Texture2D::LoadFromFile("images/grassSpec.png");
		Texture2D::sptr box = Texture2D::LoadFromFile("images/box.bmp");
		Texture2D::sptr boxSpec = Texture2D::LoadFromFile("images/box-reflections.bmp");
		Texture2D::sptr bone = Texture2D::LoadFromFile("images/bone.jpg");
		Texture2D::sptr boneSpec = Texture2D::LoadFromFile("images/boneSpec.png");
		LUT3D cube("cubes/NeutralLUT.cube");
		LUT3D nuetralCube("cubes/NeutralLUT.cube");
		LUT3D warmCube("cubes/WarmLUT.cube");
		LUT3D coolCube("cubes/CoolLUT.cube");
		LUT3D customCube("cubes/CustomLUT.cube");

		// Load the cube map
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/sample.jpg");
		TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/ToonSky.jpg");

		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();  
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		// Clear it with a white colour
		texture2->Clear();

		#pragma endregion

		///////////////////////////////////// Scene Generation //////////////////////////////////////////////////
		#pragma region Scene Generation
		
		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		// Create a scene, and set it to be the active scene in the application
		GameScene::sptr scene = GameScene::Create("test");
		Application::Instance().ActiveScene = scene;

		// We can create a group ahead of time to make iterating on the group faster
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroup =
			scene->Registry().group<RendererComponent>(entt::get_t<Transform>());

		// Create a material and set some properties for it
		ShaderMaterial::sptr sandStoneMat = ShaderMaterial::Create();
		sandStoneMat->Shader = shader;
		sandStoneMat->Set("s_Diffuse", sandStone);
		sandStoneMat->Set("s_Specular", sandStoneSpec);
		sandStoneMat->Set("u_Shininess", 2.0f);
		sandStoneMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr stoneMat = ShaderMaterial::Create();
		stoneMat->Shader = shader;
		stoneMat->Set("s_Diffuse", stone);
		stoneMat->Set("s_Specular", stoneSpec);
		stoneMat->Set("u_Shininess", 2.0f);
		stoneMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr grassMat = ShaderMaterial::Create();
		grassMat->Shader = sinShader;
		grassMat->Set("s_Diffuse", grass);
		grassMat->Set("s_Specular", grassSpec);
		grassMat->Set("u_Shininess", 2.0f);
		grassMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr boxMat = ShaderMaterial::Create();
		boxMat->Shader = shader;
		boxMat->Set("s_Diffuse", box);
		boxMat->Set("s_Specular", boxSpec);
		boxMat->Set("u_Shininess", 8.0f);
		boxMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr boneMat = ShaderMaterial::Create();
		boneMat->Shader = shader;
		boneMat->Set("s_Diffuse", bone);
		boneMat->Set("s_Specular", boneSpec);
		boneMat->Set("u_Shininess", 8.0f);
		boneMat->Set("u_TextureMix", 0.0f);

		GameObject obj1 = scene->CreateEntity("Ground"); 
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/plane.obj");
			obj1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(grassMat);
		}

		GameObject obj2 = scene->CreateEntity("Chest");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/treasureChest.obj");
			obj2.emplace<RendererComponent>().SetMesh(vao).SetMaterial(boxMat);
			obj2.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			obj2.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			obj2.get<Transform>().SetLocalScale(glm::vec3(2, 2, 2));
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj2);
		}

		GameObject obj3 = scene->CreateEntity("MonkeOne");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/monkey_quads.obj");
			obj3.emplace<RendererComponent>().SetMesh(vao).SetMaterial(sandStoneMat);
			obj3.get<Transform>().SetLocalPosition(0.0f, 10.0f, 5.0f);
			obj3.get<Transform>().SetLocalRotation(0.0f, 0.0f, -90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj3);
		}

		GameObject obj4 = scene->CreateEntity("MonkeTwo");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/monkey_quads.obj");
			obj4.emplace<RendererComponent>().SetMesh(vao).SetMaterial(sandStoneMat);
			obj4.get<Transform>().SetLocalPosition(0.0f, -10.0f, 5.0f);
			obj4.get<Transform>().SetLocalRotation(0.0f, 0.0f, 90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj4);
		}

		GameObject obj5 = scene->CreateEntity("MonkeThree");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/monkey_quads.obj");
			obj5.emplace<RendererComponent>().SetMesh(vao).SetMaterial(sandStoneMat);
			obj5.get<Transform>().SetLocalPosition(10.0f, 0.0f, 5.0f);
			obj5.get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj5);
		}

		GameObject obj6 = scene->CreateEntity("MonkeFour");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/monkey_quads.obj");
			obj6.emplace<RendererComponent>().SetMesh(vao).SetMaterial(sandStoneMat);
			obj6.get<Transform>().SetLocalPosition(-10.0f, 0.0f, 5.0f);
			obj6.get<Transform>().SetLocalRotation(0.0f, 0.0f, 0.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj6);
		}

		std::vector<glm::vec2> allAvoidAreasFrom = { glm::vec2(-4.0f, -4.0f) };
		std::vector<glm::vec2> allAvoidAreasTo = { glm::vec2(4.0f, 4.0f) };

		std::vector<glm::vec2> rockAvoidAreasFrom = { glm::vec2(-3.0f, -3.0f), glm::vec2(-19.0f, -19.0f), glm::vec2(5.0f, -19.0f),
														glm::vec2(-19.0f, 5.0f), glm::vec2(-19.0f, -19.0f) };
		std::vector<glm::vec2> rockAvoidAreasTo = { glm::vec2(3.0f, 3.0f), glm::vec2(19.0f, -5.0f), glm::vec2(19.0f, 19.0f),
														glm::vec2(19.0f, 19.0f), glm::vec2(-5.0f, 19.0f) };
		glm::vec2 spawnFromHere = glm::vec2(-19.0f, -19.0f);
		glm::vec2 spawnToHere = glm::vec2(19.0f, 19.0f);

		EnvironmentGenerator::AddObjectToGeneration("models/skeleton.obj", boneMat, 150,
			spawnFromHere, spawnToHere, allAvoidAreasFrom, allAvoidAreasTo);
		EnvironmentGenerator::AddObjectToGeneration("models/simpleRock.obj", stoneMat, 40,
			spawnFromHere, spawnToHere, rockAvoidAreasFrom, rockAvoidAreasTo);
		EnvironmentGenerator::GenerateEnvironment();

		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(0, 3, 3).LookAt(glm::vec3(0, 0, 0));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(0, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(90.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::Bind<CameraControlBehaviour>(cameraObject);
		}

		int width, height;
		glfwGetWindowSize(BackendHandler::window, &width, &height);

		Framebuffer* colorCorrect;
		GameObject colorCorrectionObj = scene->CreateEntity("Color Correct");
		{
			colorCorrect = &colorCorrectionObj.emplace<Framebuffer>();
			colorCorrect->AddColorTarget(GL_RGBA8);
			colorCorrect->AddDepthTarget();
			colorCorrect->Init(width, height);
		}
		
		PostEffect* basicEffect;
		GameObject framebufferObject = scene->CreateEntity("Basic Effect");
		{
			basicEffect = &framebufferObject.emplace<PostEffect>();
			basicEffect->Init(width, height);
		}

		GreyscaleEffect* greyscaleEffect;
		GameObject greyscaleEffectObject = scene->CreateEntity("Greyscale Effect");
		{
			greyscaleEffect = &greyscaleEffectObject.emplace<GreyscaleEffect>();
			greyscaleEffect->Init(width, height);
		}

		SepiaEffect* sepiaEffect;
		GameObject sepiaEffectObject = scene->CreateEntity("Sepia Effect");
		{
			sepiaEffect = &sepiaEffectObject.emplace<SepiaEffect>();
			sepiaEffect->Init(width, height);
		}
		#pragma endregion 
		//////////////////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////// SKYBOX ///////////////////////////////////////////////
		{
			// Load our shaders
			Shader::sptr skybox = std::make_shared<Shader>();
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.vert.glsl", GL_VERTEX_SHADER);
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.frag.glsl", GL_FRAGMENT_SHADER);
			skybox->Link();

			ShaderMaterial::sptr skyboxMat = ShaderMaterial::Create();
			skyboxMat->Shader = skybox;  
			skyboxMat->Set("s_Environment", environmentMap);
			skyboxMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
			skyboxMat->RenderLayer = 100;

			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();
			
			GameObject skyboxObj = scene->CreateEntity("skybox");  
			skyboxObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);
		}
		////////////////////////////////////////////////////////////////////////////////////////


		// We'll use a vector to store all our key press events for now (this should probably be a behaviour eventually)
		std::vector<KeyPressWatcher> keyToggles;
		{
			// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
			// how this is implemented. Note that the ampersand here is capturing the variables within
			// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
			// use std::bind
			keyToggles.emplace_back(GLFW_KEY_T, [&]() { cameraObject.get<Camera>().ToggleOrtho(); });

			controllables.push_back(obj2);

			keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao++;
				if (selectedVao >= controllables.size())
					selectedVao = 0;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});
			keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao--;
				if (selectedVao < 0)
					selectedVao = controllables.size() - 1;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});

			keyToggles.emplace_back(GLFW_KEY_Y, [&]() {
				auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
				behaviour->Relative = !behaviour->Relative;
				});
			
			keyToggles.emplace_back(GLFW_KEY_1, [&]() {

				if (lightState) {
					shader->SetUniform("u_LightPos", glm::vec3(0, 0, -1000));
					shader->SetUniform("u_LightAttenuationLinear", float(0.019));
					shader->SetUniform("u_LightAttenuationQuadratic", float(0.5));

					sinShader->SetUniform("u_LightPos", glm::vec3(0, 0, -1000));
					sinShader->SetUniform("u_LightAttenuationLinear", float(0.019));
					sinShader->SetUniform("u_LightAttenuationQuadratic", float(0.5));

					lightState = false;
				}
				else {
					shader->SetUniform("u_LightPos", glm::vec3(0, 0, 10));
					shader->SetUniform("u_LightAttenuationLinear", float(0.0));
					shader->SetUniform("u_LightAttenuationQuadratic", float(0.0));

					sinShader->SetUniform("u_LightPos", glm::vec3(0, 0, 10));
					sinShader->SetUniform("u_LightAttenuationLinear", float(0.0));
					sinShader->SetUniform("u_LightAttenuationQuadratic", float(0.0));

					lightState = true;
				}
			});

			keyToggles.emplace_back(GLFW_KEY_2, [&]() {

				if (lightAmbientPow > 0) {
					lightAmbientPow = 0;
					lightSpecularPow = 0;
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					sinShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					sinShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				else {
					lightAmbientPow = 1;
					lightSpecularPow = 0;
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					sinShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					sinShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
			});

			keyToggles.emplace_back(GLFW_KEY_3, [&]() {

				if (lightSpecularPow > 0) {
					lightAmbientPow = 0;
					lightSpecularPow = 0;
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					sinShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					sinShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				else {
					lightAmbientPow = 0;
					lightSpecularPow = 1;
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					sinShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					sinShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
			});

			keyToggles.emplace_back(GLFW_KEY_4, [&]() {

				if (lightSpecularPow > 0) {
					lightAmbientPow = 0;
					lightSpecularPow = 0;
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					sinShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					sinShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				else {
					lightAmbientPow = 1;
					lightSpecularPow = 1;
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					sinShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					sinShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
			});

			keyToggles.emplace_back(GLFW_KEY_5, [&]() {

				if (lightSpecularPow > 0) {
					lightAmbientPow = 0;
					lightSpecularPow = 0;
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					sinShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					sinShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				else {
					lightAmbientPow = 1;
					lightSpecularPow = 1;
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					sinShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					sinShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (effectState == 0.0) {
					effectState = 1.0;
					sinShader->SetUniform("effectState", effectState);
				}
				else {
					effectState = 0.0;
					sinShader->SetUniform("effectState", effectState);
				}
			});
			keyToggles.emplace_back(GLFW_KEY_6, [&]() {

			});
			keyToggles.emplace_back(GLFW_KEY_7, [&]() {

			});
			keyToggles.emplace_back(GLFW_KEY_8, [&]() {

				if (!defaultCube) {
					cube = nuetralCube;
					defaultCube = !defaultCube;
				}
				else {
					cube = warmCube;
					defaultCube = !defaultCube;
				}
			});
			keyToggles.emplace_back(GLFW_KEY_9, [&]() {

				if (!defaultCube) {
					cube = nuetralCube;
					defaultCube = !defaultCube;
				}
				else {
					cube = coolCube;
					defaultCube = !defaultCube;
				}
			});
			keyToggles.emplace_back(GLFW_KEY_0, [&]() {

				if (!defaultCube) {
					cube = nuetralCube;
					defaultCube = !defaultCube;
				}
				else {
					cube = customCube;
					defaultCube = !defaultCube;
				}
			});
		}

		// Initialize our timing instance and grab a reference for our use
		Timing& time = Timing::Instance();
		time.LastFrame = glfwGetTime();

		bool firstHalf = false;
		bool secondHalf = true;
		float sinTime = 0.0;

		///// Game loop /////
		while (!glfwWindowShouldClose(BackendHandler::window)) {
			glfwPollEvents();

			// Update the timing
			time.CurrentFrame = glfwGetTime();
			time.DeltaTime = static_cast<float>(time.CurrentFrame - time.LastFrame);

			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;

			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(BackendHandler::window);
				}
			}

			// Iterate over all the behaviour binding components
			scene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
				// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
				for (const auto& behaviour : binding.Behaviours) {
					if (behaviour->Enabled) {
						behaviour->Update(entt::handle(scene->Registry(), entity));
					}
				}
			});

			// Clear the screen
			basicEffect->Clear();
			greyscaleEffect->Clear();
			sepiaEffect->Clear();
			colorCorrect->Clear();

			glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
			glEnable(GL_DEPTH_TEST);
			glClearDepth(1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Update all world matrices for this frame
			scene->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
				t.UpdateWorldMatrix();
			});

			sinShader->SetUniform("sinTime", sinTime);
			sinTime += 0.1;

			// Grab out camera info from the camera object
			Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = glm::inverse(camTransform.LocalTransform());
			glm::mat4 projection = cameraObject.get<Camera>().GetProjection();
			glm::mat4 viewProjection = projection * view;

			if (obj3.get<Transform>().GetLocalPosition().x < 10 && secondHalf) {
				obj3.get<Transform>().SetLocalRotation(0.0f, 0.0f, -90.0f);
				obj4.get<Transform>().SetLocalRotation(0.0f, 0.0f, 90.0f);
				obj5.get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
				obj6.get<Transform>().SetLocalRotation(0.0f, 0.0f, 0.0f);
				obj3.get<Transform>().MoveLocalFixed(0.025, 0, 0);
				obj4.get<Transform>().MoveLocalFixed(-0.025, 0, 0);
				obj5.get<Transform>().MoveLocalFixed(0, -0.025, 0);
				obj6.get<Transform>().MoveLocalFixed(0, 0.025, 0);
			}
			else if (obj3.get<Transform>().GetLocalPosition().y > -10 && secondHalf) {
				obj3.get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
				obj4.get<Transform>().SetLocalRotation(0.0f, 0.0f, 0.0f);
				obj5.get<Transform>().SetLocalRotation(0.0f, 0.0f, 90.0f);
				obj6.get<Transform>().SetLocalRotation(0.0f, 0.0f, -90.0f);
				obj3.get<Transform>().MoveLocalFixed(0, -0.025, 0);
				obj4.get<Transform>().MoveLocalFixed(0, 0.025, 0);
				obj5.get<Transform>().MoveLocalFixed(-0.025, 0, 0);
				obj6.get<Transform>().MoveLocalFixed(0.025, 0, 0);
			}
			else {
				firstHalf = true;
				secondHalf = false;
			}

			if (obj3.get<Transform>().GetLocalPosition().x > -10 && firstHalf) {
				obj3.get<Transform>().SetLocalRotation(0.0f, 0.0f, 90.0f);
				obj4.get<Transform>().SetLocalRotation(0.0f, 0.0f, -90.0f);
				obj5.get<Transform>().SetLocalRotation(0.0f, 0.0f, 0.0f);
				obj6.get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
				obj3.get<Transform>().MoveLocalFixed(-0.025, 0, 0);
				obj4.get<Transform>().MoveLocalFixed(0.025, 0, 0);
				obj5.get<Transform>().MoveLocalFixed(0, 0.025, 0);
				obj6.get<Transform>().MoveLocalFixed(0, -0.025, 0);
			}
			else if (obj3.get<Transform>().GetLocalPosition().y < 10 && firstHalf) {
				obj3.get<Transform>().SetLocalRotation(0.0f, 0.0f, 0.0f);
				obj4.get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
				obj5.get<Transform>().SetLocalRotation(0.0f, 0.0f, -90.0f);
				obj6.get<Transform>().SetLocalRotation(0.0f, 0.0f, 90.0f);
				obj3.get<Transform>().MoveLocalFixed(0, 0.025, 0);
				obj4.get<Transform>().MoveLocalFixed(0, -0.025, 0);
				obj5.get<Transform>().MoveLocalFixed(0.025, 0, 0);
				obj6.get<Transform>().MoveLocalFixed(-0.025, 0, 0);
			}
			else {
				secondHalf = true;
				firstHalf = false;
			}

			// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
			renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
				// Sort by render layer first, higher numbers get drawn last
				if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
				if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

				// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
				if (l.Material->Shader < r.Material->Shader) return true;
				if (l.Material->Shader > r.Material->Shader) return false;

				// Sort by material pointer last (so we can minimize switching between materials)
				if (l.Material < r.Material) return true;
				if (l.Material > r.Material) return false;
				
				return false;
			});

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			colorCorrect->Bind();

			// Iterate over the render group components and draw them
			renderGroup.each( [&](entt::entity e, RendererComponent& renderer, Transform& transform) {
				// If the shader has changed, set up it's uniforms
				if (current != renderer.Material->Shader) {
					current = renderer.Material->Shader;
					current->Bind();
					BackendHandler::SetupShaderForFrame(current, view, projection);
				}
				// If the material has changed, apply it
				if (currentMat != renderer.Material) {
					currentMat = renderer.Material;
					currentMat->Apply();
				}
				// Render the mesh
				BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
			});

			colorCorrect->Unbind();

			colorCorrectionShader->Bind();

			colorCorrect->BindColorAsTexture(0, 0);
			cube.bind(30);

			colorCorrect->DrawFullscreenQuad();

			cube.unbind(30);
			colorCorrect->UnbindTexture(0);

			colorCorrectionShader->UnBind();

			/*sepiaEffect->ApplyEffect(basicEffect);

			sepiaEffect->DrawToScreen();*/

			// Draw our ImGui content
			BackendHandler::RenderImGui();

			scene->Poll();
			glfwSwapBuffers(BackendHandler::window);
			time.LastFrame = time.CurrentFrame;
		}

		// Nullify scene so that we can release references
		Application::Instance().ActiveScene = nullptr;
		//Clean up the environment generator so we can release references
		EnvironmentGenerator::CleanUpPointers();
		BackendHandler::ShutdownImGui();
	}	

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}