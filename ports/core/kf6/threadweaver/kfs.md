## About

ThreadWeaver is a job queue for spreading work over a pool of threads: a program describes its work
as jobs, collections and sequences with dependencies between them, and ThreadWeaver runs each when
its turn comes on the next free thread. [okular](#port-okular), [kdevelop](#port-kdevelop) and
[digikam](#port-digikam) use it. It needs nothing beyond Qt Core.

## Notes

The source carries no translations, so the recipe has no `KF_SKIP_PO_PROCESSING`.
