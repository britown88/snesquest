layout(std140, binding = 0) uniform uboView{
   mat4 uViewMatrix;
};

#ifdef FRAGMENT

out vec4 outColor;
in vec4 vColor;

#ifdef DIFFUSE_TEXTURE
uniform sampler2D uTexture;
in vec2 vTexCoords;
#endif

void main(){
   vec4 color = vColor;

   #ifdef DIFFUSE_TEXTURE
   color *= texture(uTexture, vTexCoords);
   #endif
      
   outColor = color;
}

#endif

#ifdef VERTEX
uniform mat4 uModelMatrix;
uniform vec4 uColorTransform;

#ifdef ROTATION
uniform mat4 uModelRotation;
#endif

in vec2 aPosition;
#ifdef COLOR_ATTRIBUTE
in vec4 aColor;
#endif
out vec4 vColor;

#ifdef DIFFUSE_TEXTURE
uniform mat4 uTexMatrix;
in vec2 aTexCoords;
out vec2 vTexCoords;
#endif

void main() {
   #ifdef COLOR_ATTRIBUTE
	vColor = aColor * uColorTransform;
   #else
	vColor = uColorTransform;
   #endif
      
   #ifdef DIFFUSE_TEXTURE
   vec4 coord = vec4(aTexCoords, 0.0, 1.0);
   coord = uTexMatrix * coord;
   vTexCoords = coord.xy;
   #endif

   vec4 position = vec4(aPosition, 0, 1);
   mat4 model = uModelMatrix;
   #ifdef ROTATION
	model *= uModelRotation;
   #endif
	  
   gl_Position = uViewMatrix * (model * position);
   //gl_Position = position;
}
#endif