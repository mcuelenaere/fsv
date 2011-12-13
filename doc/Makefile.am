HTML_DOCS = \
	about.html \
	fsv.html \
	menus.html \
	mouse.html \
	startup.html \
	window.html \
	t1.html

PNG_IMAGES = \
	back.png \
	beyeview.png \
	cd-root.png \
	cd-up.png \
	dirtree.png \
	filelist.png \
	mapv.png \
	treev.png \
	viewport.png \
	window.png

all: $(HTML_DOCS)

$(HTML_DOCS): custom.dsl fsv.sgml
	-jade -d $(srcdir)/custom.dsl -t sgml -i html fsv.sgml
	cp -f t1.html fsv.html

doc_DATA = $(HTML_DOCS) $(PNG_IMAGES)

EXTRA_DIST = custom.dsl fsv.sgml $(HTML_DOCS) $(PNG_IMAGES)
