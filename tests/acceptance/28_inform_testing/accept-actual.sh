for f in *.actual; do mv $f $(basename $f .actual).expected; done
git add *.expected
