TEX_SOURCE      = manual.tex
TEX_DEP_SOURCES = cover.tex version_no.tex copyright.tex
IMG_DOT_SOURCES = 
IMG_FIG_SOURCES = 
IMG_EPS_SOURCES = 

# other built files - should not be modified

IMG_EPS_GENER   = $(IMG_FIG_SOURCES:%.fig=%.eps) \
                  $(IMG_DOT_SOURCES:%.dot=%.eps)
IMG_EPS_FILES   = $(IMG_EPS_SOURCES) $(IMG_EPS_GENER)
DVI_FILE        = $(TEX_SOURCE:%.tex=%.dvi)
PS_FILE         = $(TEX_SOURCE:%.tex=%.ps)
PDF_FILE        = $(TEX_SOURCE:%.tex=%.pdf)
IMG_PDF_FILES   = $(IMG_EPS_FILES:%.eps=%.pdf)
HTML_FILE       = $(TEX_SOURCE:%.tex=%.html)


.SUFFIXES:
.SUFFIXES: .html .dvi .ps .pdf .eps .fig .dot .bib .tex


all: $(DVI_FILE) $(PS_FILE) $(PDF_FILE) $(HTML_FILE)

$(DVI_FILE):  $(TEX_SOURCE) $(TEX_DEP_SOURCES) $(IMG_EPS_FILES)
$(PS_FILE):   $(DVI_FILE)
$(PDF_FILE):  $(TEX_SOURCE) $(TEX_DEP_SOURCES) $(IMG_PDF_FILES)
$(HTML_FILE): $(TEX_SOURCE) $(TEX_DEP_SOURCES) $(IMG_EPS_FILES)


.dot.eps:
	dot -Tps < $*.dot > $*.eps

.fig.eps:
	fig2dev -L eps $*.fig > $*.eps

.eps.pdf:
	epstopdf $<

.dvi.ps:
	dvips $< -o $*.ps

.tex.dvi:
	do_latex -dvi $<

.tex.pdf:
	do_latex -pdf $<

.tex.html:
	hevea -O -s -fix article.hva $<

clean: clean-gen
	rm -f *.dvi *.ps *.pdf *.html

clean-gen: clean-aux
	rm -f *.gif $(IMG_EPS_GENER)

clean-aux:
	rm -f *.dvi.* *.pdf.* *.aux *.log *.out *.bbl *.blg *.err.*
	rm -f *.htoc *.haux *.image.tex

install: all
	ssh beaune mkdir -p $$F 2>/dev/null; \
	F=public_html/$$D; \
	scp $(PS_FILE) $(PDF_FILE) $(HTML_FILE) *.gif beaune:$$F/.; \
	true


# my customisations

version_no.tex: ../VERSION
	sed -n -e 's/.*-\([0-9].*\)/\1/gp' ../VERSION >version_no.tex

