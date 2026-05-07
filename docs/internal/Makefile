PDF = internal_docs.pdf
SRC = internal_docs.tex

.PHONY: all clean

all: $(PDF)

$(PDF): $(SRC) ch_intro.tex ch_architecture.tex ch_args.tex \
               ch_bitio.tex ch_model.tex ch_scanner.tex \
               ch_lzss.tex ch_codec.tex ch_main.tex ch_improvements.tex
	pdflatex -interaction=nonstopmode $(SRC)
	pdflatex -interaction=nonstopmode $(SRC)

clean:
	rm -f *.pdf *.aux *.log *.toc *.out
