"use client";
import React, { useState, useMemo, useCallback, useRef, useEffect } from 'react';
import { Plus, Edit, Trash2, X, Save, Search } from 'lucide-react';

// Default profile object for form state
const defaultProfile = {
  id: '',
  name: '',
  email: '',
  position: '',
  department: '',
  status: 'Active',
};

// Memoized Form Components to prevent re-renders
const FormInput = React.memo(({ label, inputRef, ...props }) => (
  <div>
    <label className="block text-sm font-medium text-slate-300 mb-1">{label}</label>
    <input ref={inputRef} {...props} className="w-full p-2 bg-slate-900 border border-slate-600 rounded-md text-slate-200 focus:ring-2 focus:ring-blue-500 focus:border-blue-500" />
  </div>
));

const FormTextarea = React.memo(({ label, textareaRef, ...props }) => (
  <div>
    <label className="block text-sm font-medium text-slate-300 mb-1">{label}</label>
    <textarea ref={textareaRef} {...props} className="w-full p-2 bg-slate-900 border border-slate-600 rounded-md text-slate-200 focus:ring-2 focus:ring-blue-500 focus:border-blue-500" />
  </div>
));

// Stable ProfileCard component
const ProfileCard = React.memo(({ profile, handleEdit, handleDelete }) => (
  <div className="bg-slate-800 rounded-2xl p-5 border border-slate-700 hover:border-blue-500 transition-colors duration-300 group relative">
    <div className="absolute top-4 right-4 flex space-x-2 opacity-0 group-hover:opacity-100 transition-opacity z-10">
      <button onClick={() => handleEdit(profile)} className="p-2 text-slate-400 hover:text-white hover:bg-slate-700 rounded-full transition-colors"><Edit size={20} /></button>
      <button onClick={() => handleDelete(profile.id)} className="p-2 text-slate-400 hover:text-red-500 hover:bg-red-500/10 rounded-full transition-colors"><Trash2 size={20} /></button>
    </div>
    <div className="flex items-center space-x-4">
      <img
        src={profile.avatar || `https://ui-avatars.com/api/?name=${profile.name.replace(' ', '+')}&background=0D89EC&color=fff`}
        alt={profile.name}
        className="w-16 h-16 rounded-full object-cover border-2 border-slate-600"
        onError={e => { e.target.onerror = null; e.target.src = `https://ui-avatars.com/api/?name=${profile.name.replace(' ', '+')}&background=0D89EC&color=fff`; }}
      />
      <div>
        <h3 className="text-xl font-bold text-slate-50 break-words max-w-[8rem]">{profile.name}</h3>
        <p className="text-blue-400 font-semibold">{profile.position}</p>
      </div>
    </div>
  </div>
));

// Stable Modal component
const Modal = React.memo(({ children, onClose, isOpen }) => {
  if (!isOpen) return null;
  return (
    <div className="fixed inset-0 bg-black/70 backdrop-blur-sm flex items-center justify-center p-4 z-50" onClick={onClose}>
      <div className="bg-slate-800 rounded-2xl max-w-2xl w-full max-h-[90vh] overflow-y-auto border border-slate-700 shadow-2xl" onClick={e => e.stopPropagation()}>
        {children}
      </div>
    </div>
  );
});

const ProfileDashboard = () => {  // Create refs for form inputs
  const idInputRef = useRef(null);
  const nameInputRef = useRef(null);
  const emailInputRef = useRef(null);
  const positionInputRef = useRef(null);
  const departmentInputRef = useRef(null);
  
  const [profiles, setProfiles] = useState([]);
  const [isFormOpen, setIsFormOpen] = useState(false);
  const [editForm, setEditForm] = useState(defaultProfile);
  const [searchTerm, setSearchTerm] = useState('');
  
  // Store the active input field
  const [activeField, setActiveField] = useState(null);

  // Fetch profiles from API
  useEffect(() => {
    fetch('/api/users')
      .then(res => res.json())
      .then(data => setProfiles(data))
      .catch(err => console.error('Failed to fetch users:', err));
  }, []);

  // Fetch roles from API
  const [roles, setRoles] = useState([]);
  useEffect(() => {
    fetch('/api/roles')
      .then(res => res.json())
      .then(data => setRoles(data))
      .catch(err => console.error('Failed to fetch roles:', err));
  }, []);

  const filteredProfiles = useMemo(() =>
    profiles.filter(profile =>
      profile.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      profile.position.toLowerCase().includes(searchTerm.toLowerCase()) ||
      profile.department.toLowerCase().includes(searchTerm.toLowerCase())
    ), [profiles, searchTerm]);
  const handleEdit = useCallback((profile) => {
    setEditForm({ ...profile });
    setIsFormOpen(true);
  }, []);
  // Thêm hàm thêm/sửa gọi API Next.js
  const handleSave = useCallback(() => {
    const updatedProfileData = { ...editForm };
    if (editForm.id) {
      // Update user
      fetch('/api/users', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(updatedProfileData)
      })
        .then(res => res.json())
        .then(data => {
          setProfiles(prev => prev.map(p => p.id === data.id ? data : p));
          closeModals();
        });
    } else {
      // Create user
      fetch('/api/users', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(updatedProfileData)
      })
        .then(res => res.json())
        .then(data => {
          setProfiles(prev => [...prev, data]);
          closeModals();
        });
    }
  }, [editForm]);
  const handleDelete = useCallback((id) => {
    if (window.confirm('Are you sure you want to delete this profile?')) {
      fetch(`/api/users?id=${id}`, { method: 'DELETE' })
        .then(res => res.json())
        .then(() => setProfiles(prev => prev.filter(p => p.id !== id)));
    }
  }, []);

  const handleAddNew = useCallback(() => {
    setEditForm(defaultProfile);
    setIsFormOpen(true);
  }, []);
  const closeModals = useCallback(() => {
    setIsFormOpen(false);
    setEditForm(defaultProfile);
    setActiveField(null);
  }, []);

  // Handle form field changes with focus retention
  const handleFormChange = useCallback((field, value) => {
    setEditForm(prev => ({ ...prev, [field]: value }));
    
    // Keep track of which field is active
    setActiveField(field);
    
    // Restore focus to the appropriate input after render
    setTimeout(() => {
      if (field === 'id' && idInputRef.current) idInputRef.current.focus();
      if (field === 'name' && nameInputRef.current) nameInputRef.current.focus();
      if (field === 'email' && emailInputRef.current) emailInputRef.current.focus();
      if (field === 'position' && positionInputRef.current) positionInputRef.current.focus();
      if (field === 'department' && departmentInputRef.current) departmentInputRef.current.focus();
    }, 0);
  }, []);

  return (
    <div className="min-h-screen bg-slate-900 text-slate-300">
      <header className="bg-slate-800/80 backdrop-blur-lg border-b border-slate-700 sticky top-0 z-40">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-4">
          <div className="flex justify-between items-center">
            <h1 className="text-3xl font-bold text-slate-50">
              Profile Dashboard
            </h1>
            <button
              onClick={handleAddNew}
              className="bg-blue-600 text-white px-4 py-2 rounded-lg hover:bg-blue-700 transition-colors flex items-center space-x-2 font-semibold"
            >
              <Plus className="w-5 h-5" />
              <span>Add Profile</span>
            </button>
          </div>
        </div>
      </header>

      <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        <div className="flex flex-col sm:flex-row justify-between items-center gap-4 mb-8">
          <div className="bg-slate-800 rounded-xl p-4 border border-slate-700">
             <p className="text-slate-400">Total Profiles</p>
             <div className="text-3xl font-bold text-slate-50">{profiles.length}</div>
          </div>
          <div className="relative w-full max-w-xs">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 text-slate-500 w-5 h-5" />
            <input
              type="text"
              placeholder="Search profiles..."
              onChange={e => setSearchTerm(e.target.value)}
              className="w-full pl-10 pr-4 py-2 border border-slate-600 bg-slate-800 text-slate-200 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
          </div>
        </div>
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-6">
          {filteredProfiles.map(profile => (
            <ProfileCard
              key={profile.id}
              profile={profile}
              handleEdit={handleEdit}
              handleDelete={handleDelete}
            />
          ))}
        </div>
        {!filteredProfiles.length && (
          <div className="text-center text-slate-500 py-16">
            <p className="text-xl font-semibold">No profiles found.</p>
          </div>
        )}
      </main>      {/* View details modal removed */}
      
      <Modal isOpen={isFormOpen} onClose={closeModals}>
        <div className="p-6">
          <h2 className="text-2xl font-bold text-slate-50 mb-6">{editForm.id ? 'Edit Profile' : 'Add New Profile'}</h2>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <FormInput 
              label="ID" 
              placeholder="Enter ID (〃＞目＜)〃DO NOT LEAVE IT BLANK" 
              value={editForm.id || ''} 
              onChange={e => handleFormChange('id', e.target.value)}
              inputRef={idInputRef}
            />
            <FormInput 
              label="Name" 
              placeholder="John Doe" 
              value={editForm.name || ''} 
              onChange={e => handleFormChange('name', e.target.value)}
              inputRef={nameInputRef}
            />
            <FormInput 
              label="Email" 
              type="email" 
              placeholder="john@example.com" 
              value={editForm.email || ''} 
              onChange={e => handleFormChange('email', e.target.value)}
              inputRef={emailInputRef}
            />
            <div>
              <label className="block text-sm font-medium text-slate-300 mb-1">Position</label>
              <select
                value={editForm.role_id || ''}
                onChange={e => handleFormChange('role_id', e.target.value)}
                className="w-full p-2 bg-slate-900 border border-slate-600 rounded-md text-slate-200 focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
              >
                <option value="">Select role</option>
                {roles.map(role => (
                  <option key={role.id} value={role.id}>{role.name}</option>
                ))}
              </select>
            </div>
            <FormInput 
              label="Department" 
              placeholder="Engineering" 
              value={editForm.department || ''} 
              onChange={e => handleFormChange('department', e.target.value)}
              inputRef={departmentInputRef}
            />
          </div>
          <div className="flex justify-end space-x-3 mt-6 pt-6 border-t border-slate-700">
            <button onClick={closeModals} className="px-4 py-2 bg-slate-700 text-slate-200 rounded-lg hover:bg-slate-600 transition-colors">Cancel</button>
            <button onClick={handleSave} className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors">{editForm.id ? 'Update' : 'Create'}</button>
          </div>
        </div>
      </Modal>

    </div>
  );
};

export default ProfileDashboard;