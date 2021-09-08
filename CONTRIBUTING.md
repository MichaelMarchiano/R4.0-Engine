# How to contribute efficiently

**Please read the first section before reporting a bug!**

## Reporting bugs or proposing features

The golden rule is to **always open *one* issue for *one* bug**. If you notice several bugs and want to report them, make sure to create one new issue for each of them.

Everything refered to hereafter as "bug" also applies for feature requests.

If you are reporting a new issue, you will make our life much simpler (and the fix come much sooner) by following those guidelines:

#### Search first in the existing database

Issues are often reported several times by various users. It's a good practice to **search first** in the issues database before reporting your issue. If you don't find a relevant match or if you are unsure, don't hesitate to **open a new issue**. The bugsquad will handle it from there if it's a duplicate.

#### Specify the platform 
 
Godot runs on a large variety of platforms and operating systems and devices. If you believe your issue is device/platform dependent (for example if it is related to the rendering, crashes or compilation errors), please specify:
* Operating system
* Device (including architecture, e.g. x86, x86_64, arm, etc.)
* GPU model (and driver in use if you know it)

#### Specify steps to reproduce

Many bugs can't be reproduced unless specific steps are taken. Please **specify the exact steps** that must be taken to reproduce the condition, and try to keep them as minimal as possible.

#### Provide a simple, example project

Sometimes an unexpected behavior happens in your project. In such case, understand that:

* What happens to you may not happen to other users.
* We can't take the time to look at your project, understand how it is set up and then figure out why it's failing.
 
To speed up our work, please prepare for us **a simple project** that isolates and reproduces the issue. This is always the **the best way for us to fix it**. You can attach a zip file with the minimal project directly to the bug report, by drag and dropping the file in the GitHub edition field.

## Contributing pull requests

If you want to add new engine functionalities, please make sure that:

* This functionality is desired.
* You talked to other developers on how to implement it best (on either communication channel, and maybe in a GitHub issue first before making your PR).
* Even if it does not get merged, your PR is useful for future work by another developer.

Similar rules can be applied when contributing bug fixes - it's always best to discuss the implementation in the bug report first if you are not 100% about what would be the best fix.

#### Be nice to the git history

Try to make simple PRs with that handle one specific topic. Just like for reporting issues, it's better to open 3 different PRs that each address a different issue than one big PR with three commits.

When updating your fork with upstream changes, please use ``git pull --rebase`` to avoid creating "merge commits". Those commits unnecessarily pollute the git history when coming from PRs.

Also try to make commits that bring the engine from one stable state to another stable state, i.e. if your first commit has a bug that you fixed in the second commit, try to merge them together before making your pull request (see ``git rebase -i`` and relevant help about rebasing or ammending commits on the Internet).

This git style guide has some good practices to have in mind: https://github.com/agis-/git-style-guide

Thanks!

The Godot development team
