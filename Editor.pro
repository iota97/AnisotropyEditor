QT += widgets
DESTDIR = $$PWD

HEADERS += \
    src/Texture/cubemap.h \
    src/Field/optimizer.h \
    src/Field/anisotropy.h \
    src/Render/constraints.h \
    src/Render/montecarlo.h \
    src/Render/mesh.h \
    src/Render/objloader.h \
    src/Render/render.h \
    src/Texture/texture.h \
    src/UI/constraintseditor.h \
    src/UI/menubar.h \
    src/UI/newproject.h \
    src/UI/renderingsettings.h \
    src/UI/optimizersettings.h \
    src/UI/toolbar.h \
    src/UI/bottombar.h \
    src/UI/vulkanwindow.h \
    src/UI/mainapp.h

SOURCES += \
    src/Texture/cubemap.cpp \
    src/Field/optimizer.cpp \
    src/Field/anisotropy.cpp \
    src/Render/constraints.cpp \
    src/Render/montecarlo.cpp \
    src/Render/mesh.cpp \
    src/Render/objloader.cpp \
    src/Render/render.cpp \
    src/Texture/texture.cpp \
    src/UI/constraintseditor.cpp \
    src/UI/menubar.cpp \
    src/UI/newproject.cpp \
    src/UI/renderingsettings.cpp \
    src/UI/optimizersettings.cpp \
    src/UI/toolbar.cpp \
    src/UI/bottombar.cpp \
    src/UI/vulkanwindow.cpp \
    src/UI/mainapp.cpp \
    src/main.cpp

SHADERS += \
    src/Field/Shaders/dir2cov.comp \
    src/Field/Shaders/dir2zebra.comp \
    src/Field/Shaders/dir2dir.comp \
    src/Field/Shaders/half2dir.comp \
    src/Field/Shaders/imageImporter.comp \
    src/Field/Shaders/imageExporter.comp \
    src/Field/Shaders/init.vert \
    src/Field/Shaders/init.frag \
    src/Field/Shaders/init.tesc \
    src/Field/Shaders/init.tese \
    src/Field/Shaders/init.comp \
    src/Field/Shaders/smooth.comp \
    src/Field/Shaders/finalize.comp \
    src/Field/Shaders/restrict.comp \
    src/Field/Shaders/prolong.comp \
    src/Texture/Shaders/cubeIntegrator.comp \
    src/Render/Shaders/accurate.vert \
    src/Render/Shaders/accurate.frag \
    src/Render/Shaders/fast.vert \
    src/Render/Shaders/fast.frag \
    src/Render/Shaders/bent.vert \
    src/Render/Shaders/bent.frag \
    src/Render/Shaders/direction.vert \
    src/Render/Shaders/direction.frag \
    src/Render/Shaders/twilight.vert \
    src/Render/Shaders/twilight.frag \
    src/Render/Shaders/sine.vert \
    src/Render/Shaders/sine.frag \
    src/Render/Shaders/reference.vert \
    src/Render/Shaders/reference.frag \
    src/Render/Shaders/resolve.vert \
    src/Render/Shaders/resolve.frag \
    src/Render/Shaders/copy.vert \
    src/Render/Shaders/copy.frag \
    src/Render/Shaders/constraint.vert \
    src/Render/Shaders/constraint.frag \
    src/Render/Shaders/constraint.tese \
    src/Render/Shaders/constraint.tesc \
    src/Render/Shaders/handle.vert \
    src/Render/Shaders/handle.frag

createShaderDir.target = $$PWD/assets/shaders
createShaderDir.commands = $(MKDIR) $$shell_path($$PWD/assets/shaders)
QMAKE_EXTRA_TARGETS += createShaderDir
PRE_TARGETDEPS += $$PWD/assets/shaders

for (in, SHADERS) {
    nameList = $$split(in, ".")
    ext = $$member(nameList, -1, -1)
    nameListNoExt = $$member(nameList, 0, -2)
    shader = $$join(nameListNoExt, ".")

    name = compile_$$in
    target = "compile_$${in}.target"
    commands = "compile_$${in}.commands"
    depends = "compile_$${in}.depends"

    $$target = $$PWD/assets/shaders/$$basename(shader)_$${ext}.spv
    $$commands = glslc $$PWD/$${shader}.$${ext} -o $$PWD/assets/shaders/$$basename(shader)_$${ext}.spv
    $$depends = $$PWD/$${shader}.$${ext}
    QMAKE_EXTRA_TARGETS += $$name
    POST_TARGETDEPS += $$PWD/assets/shaders/$$basename(shader)_$${ext}.spv
}
