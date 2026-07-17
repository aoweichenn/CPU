LATEX_DIR := source/latex

.PHONY: all check pdf text-count text-count-chapters text-target clean

all: pdf

check:
	$(MAKE) -C $(LATEX_DIR) check

pdf:
	$(MAKE) -C $(LATEX_DIR) pdf

text-count:
	$(MAKE) -C $(LATEX_DIR) text-count

text-count-chapters:
	$(MAKE) -C $(LATEX_DIR) text-count-chapters

text-target:
	$(MAKE) -C $(LATEX_DIR) text-target

clean:
	$(MAKE) -C $(LATEX_DIR) clean
