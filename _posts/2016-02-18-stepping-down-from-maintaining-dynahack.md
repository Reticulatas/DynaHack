---
title: Stepping down from maintaining DynaHack
author: tungtn
date: 2016-02-18 18:17:13 +1100
tags: [dynahack, nethack, unnethack, nethack4, nitrohack]
---
As mentioned in the [DynaHack 0.6.0 release post]({{ site.baseurl }}{% post_url 2016-02-18-dynahack-0-6-0-released %}), I will no longer be developing or maintaining DynaHack.  Anybody who's been following development has seen this coming a long way away; there hasn't been any meaningful development [for at least six months](https://github.com/tung/DynaHack/commits/unnethack).  For anybody who hasn't this whole thing may come as a bit of a surprise.  In either case I feel an explanation is in order.

The simple explanation is that I've burned out on DynaHack.  Just thinking about doing anything related to DynaHack makes me feel exhausted.  For example, there's a `verbose` option that's in the game that comes all the way from the original NetHack, which can be disabled to give much less message spam.  Even in NetHack most people don't touch it, because it can suppress important messages, such as when a monster on Astral is swinging around Vorpal Blade.  Making artifacts produce messages even when `verbose` is disabled is I think a one-line fix, trivial.  I couldn't make myself do it in time for the 0.6.0 release.  That's the kind of burn-out I'm facing here with DynaHack.

There were a couple of other less-trivial things I really wanted for DynaHack 0.6.0 that didn't make it:

* Random vault rooms from UnNetHack 5.  NetHack and most of its variants only have rectangular rooms and corridors in the top half of its dungeon.  UnNetHack and SporkHack before it implemented vaults, specially shaped and often hand-crafted room designs that could occasionally appear in the main dungeon.  It's a feature that originally appeared in Angband, but is standard in almost every single roguelike except NetHack, and its appearance in UnNetHack deserves a lot more attention than it ended up getting.  I especially wanted it for DynaHack because it's something that people can appreciate regardless of their skill level, which is the kind of change that is sorely lacking in a lot of NetHack variants in general.
* The `#technique` command from SLASH'EM.  Yes, they have balance issues, but the most direct way of differentiating races and roles is to give each race and role unique actions only they can perform.  In my opinion the fun added by that offsets the balance issues and then some.  Fun does not come from balance alone, and even then, if techniques were added to DynaHack and I wasn't halting development, I'd have probably taken the time to address balance concerns anyway.

The main reason for the burn-out I'm experiencing with DynaHack was having to deal with bugs from its roots in [NitroHack](https://nethackwiki.com/wiki/NitroHack).  The things I've ported from UnNetHack have mostly been solid.  The things I've ported from other NetHack variants have mostly been solid.  The original things I've done from DynaHack have mostly been solid.  NitroHack, the special snowflake that it is, seems to be an endless fountain of bugs.  I've fixed almost all of the critical ones, so DynaHack is pretty stable overall.  But NitroHack bugs generally fall into two categories: things that are easy to fix, and things that are almost impossible to fix without rewriting large portions of the code.  The stuff that's easy to fix I've fixed, either by myself or by porting the relevant fix in [NetHack 4](http://nethack4.org), the other NetHack variant based on NitroHack.

That just leaves the NitroHack bugs that only huge rewrites would fix.  NetHack 4 has taken the multi-year liberty of doing this; in fact, one way of describing NetHack 4 is as a full-focus zero-gameplay-changes multi-developer multi-year effort to make NitroHack playable.  DynaHack in contrast is developed solely by yours truly, with a handful of exceptions here and there.  If there is a bug in DynaHack, I'm effectively the only person that can fix it, regardless of how well or poorly I understand the portion of the code responsible for that bug.  The net results of this were:

1. The hard-to-fix NitroHack bugs became my responsibility, regardless of my ability to actually fix them, leading to
2. I became the *de facto* NitroHack maintainer.  Just to clarify, the actual NitroHack maintainer who went AWOL several years ago is Daniel Thaler and **definitely not me**.

When players run into bugs, they're not happy, and when they're not happy, I'm not happy, and this unhappiness has made working on DynaHack itself an unhappy experience.  Thus, burn-out.

Despite all this, I still like DynaHack as a game.  It stands alone amongst NetHack variants for a number of reasons:

1. DynaHack is the only NetHack variant I know of that aims specifically to eliminate tedium.
2. DynaHack has random object properties, e.g. short sword of fire, darts of detonation, leather armor of cold resistance.  Equipment selection is so limited in stock NetHack, and random object properties encourage branching out and wielding/wearing things that you'd never even look at in stock NetHack.  They were also in [GruntHack](https://nethackwiki.com/wiki/GruntHack), but that game in general is much more hostile to novice players, leading to
3. DynaHack is aimed at players of all skill levels.  Changes skew towards things that you can experience even if you're not a serial ascender.  This matters especially because NetHack variants in general have a long-standing reputation for heavily favoring expert players, the same reputation that plagues romhacks, and I wanted to use DynaHack to push back against that.

### A brief and incomplete history of DynaHack

As interesting as it would be, I don't know if I'll ever have the time or inclination to do a proper post-mortem of my time working on DynaHack, so I'll take this opportunity to talk about a few things I haven't really talked about elsewhere.

DynaHack's development was sparked by the January 2012 public release of NitroHack.  Nowadays there are many NetHack variants, but back then the only other active variant was UnNetHack; SLASH'EM was dead, and SporkHack had ceased development in 2009 (I think?).  I wanted UnNetHack's general game improvements, SporkHack's improved game balance and content and NitroHack's easy-to-work-with (at the time) code.  (Aside: NitroHack's code *is* actually easier to work with than NetHack's, based solely on the fact that NitroHack converted the code to standardized C, which can be understood by a code refactoring utility called `cscope` (vanilla NetHack's code cannot be understood by it), which is essentially god-mode for C programming).  And so on March 1, 2012, I began porting commits from UnNetHack onto NitroHack.  At the time, this was actually an achievable goal, because UnNetHack, although it was pretty much the only variant under active development, had actually stalled quite a bit, and thus it was very possible for me to catch up.  UnNetHack had about 1000 commits for me to work through, and so work I did.

One of the first things I ran into when porting UnNetHack were the new levels.  There were quite a few of them, though they're mostly random replacements for existing special levels and not straight-up new areas like in, say, [dNetHack](https://nethackwiki.com/wiki/User:Chris/dNetHack) nowadays.  Amongst the first of these new levels were the new Sokoban levels.  At the time I thought they were original UnNetHack level designs, it only much later that I discovered that they were ported from SLASH'EM.  In any case, I wanted to make sure that the new Sokoban levels were solvable, so I went about solving them, all of them, multiple times, by literally going into wizard (debug) mode and warping to Sokoban and quitting after solving the final level.  I got so good at it that I ended up writing the Sokoban solutions for those levels on the NetHack Wiki.  Keep that in mind, it'll come up again later.

And then came `lev_comp`: UnNetHack's new level compiler.  It was originally the work of Pasi Kallinen (paxed), who nowadays is on the NetHack DevTeam proper, but back then it had been implemented for SporkHack, and then brought into UnNetHack I think also by him (but don't quote me on that).  Now it was time for me to bring it into... whatever I was working on, I unofficially dubbed it "UnNitroHack", the "DynaHack" name only came much later.  That attempt to port in the new `lev_comp` nearly killed the project in its tracks.  At that point I'd been doing manual patching by hand, literally bringing up the UnNetHack change in a window on one monitor and typing what I saw in the other, at least for small- and medium-sized code changes.  This let me reformat the code to fit the code style that existed in "UnNitroHack", guaranteed I'd see the code that was running so I understood it at least on a cursory level, and helped me spot and fix bugs early before they sneaked their way into "UnNitroHack" (and there were definitely bugs).  This approach did not work for `lev_comp`.  Typing a 5000-line patch by hand wasn't just a physically awful experience (actually it was more of a controlled copy-and-paste effort), it also led to me accidentally introducing really subtle typo bugs.  Eventually I caved in and used `diff`, and in an astonishing stroke of good luck, the biggest files in UnNetHack and my own "UnNitroHack" were almost identical except for stylistic changes.  I was able to spot the errors I had introduced, fixed them, and, 16 days later I had successfully ported UnNetHack's `lev_comp` to "UnNitroHack".  Just for perspective, the changes up to that point usually measured in dozens of lines and took between minutes and hours to port.

In the process of developing "UnNitroHack", I kept an eye on the progress of UnNetHack, which was mostly stalled but occasionally got a commit or two.  Part of the reason I embarked on "UnNitroHack" was to find out more about UnNetHack and why so few people were playing it compared to NAO's ([nethack.alt.org](https://alt.org/nethack/)) fairly conservative patchset; this was part of why I went to the trouble of writing solutions to UnNetHack's new Sokoban levels.  I noticed new changes being commited to UnNetHack at a much faster pace than before; it was the work of an individual by the name of Guest41.  He mentioned in the UnNetHack IRC channel that one of the things that had stopped him from getting into UnNetHack were the lack of solutions to the new Sokoban levels, and that my solutions got him past that barrier.  Guest41 would eventually go on to be one of the biggest driving forces in UnNetHack development, and most of the reasons that UnNetHack 5 feels so different to UnNetHack 4 is due to his work.  Nowadays, easily half of all of UnNetHack's development is in its jump from version 4 to version 5.  In a weird, indirect, domino-effect way, I am distantly responsible for the existence of UnNetHack 5.

I'm skipping a lot of history, but I got very close to actually catching up to UnNetHack when porting stuff over to my own "UnNitroHack".  1000-ish commits, almost all of it ported.  I was within days of reaching the goal... and then another 300 commits landed.  At the time, UnNetHack had recently switched from Subversion to git, and bhaak (the UnNetHack maintainer) took the liberty of porting in Adeon's ice-themed Sheol branch.  I'd been working constantly at catching up to UnNetHack, and when another 300 commits landed, I lost hope of ever catching up.  The reason I didn't simply continue by porting in Sheol to "UnNitroHack"/DynaHack/whatever was because a lot of those commits contained refactorings that made sense in UnNetHack but not in my own thing, or had false starts that were fixed much later down the line, which are very hard to port because I rely on testing the code to tell whether or not I did the port correctly, but I can't do that if the commit I'm porting from is itself buggy.

I never actually got around to porting much of SporkHack to DynaHack, at least in part because a lot of the less controversial changes had already been ported into UnNetHack and therefore I'd already gotten around to.  Instead I decided to strike out on my own with a combination of my own ideas and interesting things from other variants.  I don't have much to say about that that doesn't already exist in [DynaHack's commit log](https://github.com/tung/DynaHack/commits/unnethack); I suggest using a git client to clone the repo and read it, since GitHub's commit browsing is awkward to use.  I pretty much explain the ideas and experience of writing every single change in the commit messages, so the commit log serves as an alternate development blog, and I highly encourage you to read it if you are curious at all about it.

Sadly, the first public release of DynaHack 0.5.0 was kind of the beginning of the end.  Until that point, progress on DynaHack was very rapid, I could do maybe 5-10 commits on a good day, and I could slip a commit in almost every day end-to-end.  Once it was publicly released, I had trouble getting in one commit a week.  People have the impression that DynaHack's development was slow, but the truth is that this was only true post-0.5.0; development of DynaHack in general was actually really rapid.  I really needed to just take a break, but having a public release comes with the responsibility of maintaining it, accepting bugs whether or not you actually have the energy to fix them, and the whole post-release road has led to the burn-out that has led me and the project to this post today.

### DynaHack and NetHack 4

I guess I should take some time to talk about DynaHack's relationship with NetHack 4, in particular the reason why DynaHack isn't based on NetHack 4.  Both DynaHack and NetHack 4 were spawned as off-shoots of NitroHack back in early 2012.  They both began life in "private public development": developed in public, but prior to any sort of announcements.  By the time that NetHack 4 was publicly announced (I don't remember exactly when), I kind of wanted to make DynaHack on top of it instead of NitroHack, but there were two big barriers:

1. NetHack 4's development pace at the time was very rapid and the changes themselves very broad.  Rebasing the work I'd done for DynaHack on top of it would have taken away pretty much all of the time I would need to port UnNetHack's changes!
2. Even if NetHack 4's progress wasn't so fast, rebasing would have meant also dealing with bugs that would uniquely occur just from combining NetHack 4 and DynaHack.  It's frequently the case in programming that two bug-free pieces of code can be combined and have bugs occur purely from the act of combining them.  That would have been even more work to fix those bugs on top of the constant rebasing that would be needed to simply keep up with NetHack 4, so it never happened.

NetHack 4 has a bunch of bug fixes that DynaHack made to NitroHack, and DynaHack has a bunch of bug fixes that NetHack 4 made to NitroHack.  There are quite a number of NitroHack bugs fixed in parallel in DynaHack and NetHack 4, where both fix the bug but slightly differently.  DynaHack is essentially UnNetHack plus lots of original gameplay changes on top of NitroHack with enough emergency patches to deal with the worst of NitroHack's flaws.  NetHack 4 has rewritten a lot of the original NitroHack logic, sometimes multiple times, in order to make it playable.  I imagine that NetHack 4 was what Daniel Thaler had in mind when he started NitroHack.

Finally, I take responsibility for being the person who coined the name "NetHack4" (no space).  I don't remember if it was a post to RGRN (rec.games.roguelike.nethack), or a contribution to the NetHack Wiki, but I came up with it in the hopes that it would be more palatable to people who complained about the name "NetHack 4".  It was inspired by the name "TOME4", a distant Angband variant that was loosely based on a previous Angband variant with the "TOME" name but completely different in gameplay, content and style (since then, TOME4 dropped the "4", since it has many more players nowadays than the previous TOME versions).  Despite the claims of some old fuddy-duddies, the official name of NetHack 4 has always been "NetHack 4", including the space.  Apologies for the confusion.

### The NetHack source code leak

As you may (or may not) be aware, the official NetHack development has finally resumed, and it's looking the healthiest it's been in over 12 years.  It even has a [public git repository](https://github.com/NetHack/NetHack) now!  What you may not know is that a lot of this progress is due to NetHack 4, or at least its existence.

Eric S. Raymond, open-source developer and advocate, was the author of the bulk of the official NetHack Guidebook, which ships with the game even today.  I don't recall the exact chronology of events, but at some point I think in September 2014, Eric managed to get a copy of the in-development version of the source code of NetHack official.  This was a really big deal: over 10 years of both code and development history was suddenly available to people outside of the DevTeam.  It wasn't the general public though.  It was a bunch of current NetHack variant developers.  This group of people included me.

But a certain somebody by the name of kerio (one of the public NetHack server admins for acehack.de, which later became nethack.xd.cm, which later became [nethack.dank.ninja](https://nethack.dank.ninja)) also got his hands on it; in fact he's the person who made the source code available to me at first.  The cheeky rascal that he is, he set up "NetHack 3.5" on acehack.de, added some NAO patches, and even set up an announcement bot for it with a rather cute nickname: "Assangely".  He's the one who first gave a copy of the source code to me, before anybody else.

Anyway, I was invited onto a private mailing list for NetHack variant developers, made specifically for discussing the NetHack source code leak and the future of the NetHack project in general.  The response of the DevTeam to Eric's actions were varied: a couple of people were very upset, a few were happy that the code was out so that *somebody* could do *something* with it, and the rest were somewhere in between.  In the ensuing discussion between the DevTeam, Eric and the variant developers, Eric insisted (quite passionately) that maintainer ship of NetHack proper be handed over to the community.  The DevTeam folk that were still around to respond resisted this proposal; the rest of us wanted to account for the DevTeam's opinions before doing anything hastily.  That's all the communication from Eric I saw about the matter, but discussion between DevTeam and variant developers continued.  I was mostly just a lurker in all of this.

The tone that Eric left did make things very awkward on the variant developers side.  With the threat of a hostile takeover at the back of the DevTeam's mind, discussions felt like they were taking place on top of eggshells; one wrong word or thing interpreted the wrong way by a DevTeam member could mean that we could be blocked out of discussions; the DevTeam's use of silence (accidental or deliberate) as a means of controlling discussion is extremely powerful, and we all knew it.  In the worst case they could have cloistered up entirely, or resumed development in their multi-year, zero-communication style with no meaningful way for the community to contribute changes or provide feedback.  The fear amongst us that the DevTeam would simply fall back into their old ways was very real.

Of course, the actual news of the leak had reached the community at large, and it was very frustrating for them to know that this was going on behind closed doors.  It was even more frustrating for us variant developers who really wanted to share what was going on but were afraid that the DevTeam would react by killing off communications entirely.

Thankfully, Sean Hunt (coppro), at the time NetHack 4 co-developer and currently in the official NetHack DevTeam, put a lot of work into smoothing things over  He even went to visit <strike>Michael Allison</strike> Mike Stephenson, one of the DevTeam members, in person.  That eventually led to the leak announcement in September 2014, eventually followed by a number of announcements from the DevTeam to the public over the course of 2015, culminating to the December 8, 2015 release of NetHack 3.6.0, up to the git repository being made public on February 12, 2016.

I guess Alex Smith (ais523), development lead for NetHack 4, got his wish in the end: NetHack 4 really did eventually provoke the DevTeam into action again, in a sort of round-about way.

### Closing thoughts

That was quite a diversion from the topic of DynaHack, but if you're a developer and you're interested in basing work on DynaHack or even continuing it with its name, you are more than welcome to.  I probably won't be able to answer any deep technical questions about it or provide any tech support since I'm exhausted enough from working on it as-is, but I wish you all the best.  If you make any substantial progress, let me know and I can link to it from this site.