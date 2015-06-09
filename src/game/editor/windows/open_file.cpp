/*
#include "open_file.h"
#include "../../framework/exception.h"
#include "../../framework/misc.h"

#include <CEGUIWindow.h>
#include <elements/CEGUIPushButton.h>
#include <elements/CEGUIMultiColumnList.h>
#include <elements/CEGUIListboxTextItem.h>
#include <elements/CEGUIEditBox.h>

static std::string format_file_size(uint32_t size);

namespace ed {

	open_file_window *open_file = 0;

	open_file_window::open_file_window()
		: window("OpenFile")
	{
	}

	open_file_window::~open_file_window()
	{
	}

	void open_file_window::initialise()
	{
		window::initialise();

		subscribe("OpenFile/OK", CEGUI::PushButton::EventClicked,
			CEGUI::SubscriberSlot(&open_file_window::ok_clicked, this));
		subscribe("OpenFile/Cancel", CEGUI::PushButton::EventClicked,
			CEGUI::SubscriberSlot(&open_file_window::cancel_clicked, this));

		_filename = get_child<CEGUI::Editbox>("OpenFile/Name");
		_path = get_child<CEGUI::Editbox>("OpenFile/Path");
		_file_list = get_child<CEGUI::MultiColumnList>("OpenFile/FileList");
		_file_list->addColumn("", 1, CEGUI::UDim(0.0f, 20.0f));
		_file_list->addColumn("Name", 2, CEGUI::UDim(1.0f, -115.0f));
		_file_list->addColumn("Size", 3, CEGUI::UDim(0.0f, 90.0f));
		_file_list->setWantsMultiClickEvents(true);
		CEGUI::System::getSingleton().setMultiClickTimeout(1.0);

		subscribe(_file_list, CEGUI::MultiColumnList::EventSelectionChanged,
			CEGUI::SubscriberSlot(&open_file_window::item_selection_changed, this));
		subscribe(_file_list, CEGUI::MultiColumnList::EventMouseDoubleClick,
			CEGUI::SubscriberSlot(&open_file_window::item_double_clicked, this));
	}

	void open_file_window::show()
	{
		window::show();
		refresh_filelist();
	}

	void open_file_window::hide()
	{
		window::hide();
		_file_selected.clear();
	}

	// clearing the list of file is complicated because we add user data that needs to be freed
	// (via CoTaskMemFree - it's allocated by the shell) and the items themselves need to
	// be freed as well
	void open_file_window::clear_filelist()
	{
		CEGUI::MCLGridRef ref(0, 0);
		for(ref.row = 0; ref.row < _file_list->getRowCount(); ref.row ++)
		{
			CEGUI::ListboxItem *item = _file_list->getItemAtGridReference(ref);
			fw::shell_pidl *data = static_cast<fw::shell_pidl *>(item->getUserData());
			delete data;
		}

		_file_list->resetList();
	}

	void open_file_window::refresh_filelist()
	{
		if (!_curr_folder.is_root())
		{
			// if it's not the root folder, add a ".." entry
			std::vector<std::string> values;
			values.push_back("");
			values.push_back("..");
			values.push_back("");

			fw::add_multicolumn_row(_file_list, values, 0);
		}

		for(fw::shell_folder::iterator it = _curr_folder.begin(); it != _curr_folder.end(); ++it)
		{
			fw::shell_pidl &pidl(*it);

			std::vector<std::string> values;
			values.push_back(" ");
			values.push_back(pidl.get_name());
			if (pidl.is_file())
				values.push_back(format_file_size(pidl.get_size()));
			else
				values.push_back("");

			fw::add_multicolumn_row(_file_list, values, new fw::shell_pidl(pidl));
		}

		// we also want to set the "Path" to be the full path that we're currently displaying
		_path->setText(_curr_folder.get_full_path().c_str());
	}

	bool open_file_window::item_double_clicked(CEGUI::EventArgs const &e)
	{
		ok_clicked(e);
		return false;
	}

	bool open_file_window::item_selection_changed(CEGUI::EventArgs const &)
	{
		CEGUI::ListboxItem *selected = _file_list->getFirstSelectedItem();
		if (selected == 0)
			return false;

		fw::shell_pidl *pidl = static_cast<fw::shell_pidl *>(selected->getUserData());
		if (pidl == 0)
			_filename->setText("..");
		else
			_filename->setText(pidl->get_name().c_str());

		return false;
	}

	std::string open_file_window::get_full_path() const
	{
		fw::shell_pidl *pidl = 0;

		CEGUI::ListboxItem *selected = _file_list->getFirstSelectedItem();
		if (selected != 0)
		{
			pidl = static_cast<fw::shell_pidl *>(selected->getUserData());
			if (pidl == 0)
				return "..";

			if (pidl->get_name() != std::string(_filename->getText().c_str()))
			{
				// they've typed in a different filename to the one that's selected, we'll
				// have to figure out the pidl from the name.
				pidl = 0;
			}
		}

		if (pidl == 0)
		{
			// TODO!!!
			return "";
		}

		return pidl->get_full_path();
	}

	bool open_file_window::ok_clicked(CEGUI::EventArgs const &)
	{
		fw::shell_pidl *pidl = 0;

		CEGUI::ListboxItem *selected = _file_list->getFirstSelectedItem();
		if (selected != 0)
		{
			pidl = static_cast<fw::shell_pidl *>(selected->getUserData());
			if (pidl == 0)
			{
				// if we didn't get a PIDL, that means it's the ".." entry... get the PIDL of
				// the parent folder then!
				navigate_to_folder(_curr_folder.open_parent());
				return false;
			}

			if (pidl->get_name() != std::string(_filename->getText().c_str()))
			{
				// they've typed in a different filename to the one that's selected, we'll
				// have to figure out the pidl from the name.
				pidl = 0;
			}
		}

		if (pidl == 0)
		{
			// TODO!!
			return false;
		}

		if (pidl->is_folder())
		{
			navigate_to_folder(_curr_folder.open_child(*pidl));
		}
		else
		{
			// fire the "file selected" event
			if (!!_file_selected)
				_file_selected(this);

			hide();
		}

		return false;
	}

	void open_file_window::navigate_to_folder(fw::shell_folder const &new_folder)
	{
		_curr_folder = new_folder;
		clear_filelist();
		refresh_filelist();
	}

	bool open_file_window::cancel_clicked(CEGUI::EventArgs const &)
	{
		hide();
		return true;
	}
}

std::string format_file_size(uint32_t size)
{
	if (size < 1024)
		return boost::lexical_cast<std::string>(size);
	if (size < (1024 * 1024))
		return boost::lexical_cast<std::string>(size / 1024) + " KB";

	return boost::lexical_cast<std::string>(size / (1024 * 1024)) + " MB";
}
*/
